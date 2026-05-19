// CpuIndirectRenderer 实现。
//
// 与 GpuDrivenRenderer 的差异已在头文件注释；本实现的 graphics 段完全复用
// vertex_buffer_test.vert/frag（其使用 gl_DrawID 索引 draw_meta_infos[]，使用
// visible_instance_ids[gl_InstanceIndex] 索引 instance_data[]）；
// CPU 视锥剔除是在 host 端用与 cull.comp 等价的逻辑：
//   for each (mesh m, instance i):
//     world_center = transform_i * local_center_m
//     world_radius = local_radius_m * |transform_i.column(0)|
//     if frustum_test(world_center, world_radius):
//       append (instance_pool_offset_m + i) to visible_instances
//   draw_meta_infos[m].instance_count = sum of visible per m

#include "CpuIndirectRenderer.h"

#include <chrono>
#include <utility>
#include <vector>

#include <vulkan/vulkan.hpp>

#include "Vulkan/VulkanContext.h"
#include "Vulkan/VulkanShaderProgram.h"
#include "Vulkan/VulkanSwapchain.h"
#include "Vulkan/ds/VulkanBindlessDescriptorSet.h"
#include "Vulkan/ds/VulkanDescriptorSetManager.h"
#include "Vulkan/memory/VulkanAttachment.h"
#include "Vulkan/memory/VulkanImageObject.h"
#include "ecs/Entity.h"
#include "log.h"

using lcf::ShaderTypeFlagBits;
using lcf::render::GraphicPipelineCreateInfo;
using lcf::render::VulkanAttachment;
using lcf::render::VulkanFramebufferObjectCreateInfo;
using lcf::render::VulkanShaderProgram;
using lcf::render::VulkanSwapchain;
namespace vkenums = lcf::render::vkenums;

namespace lcf::benchmark {

    namespace {
        constexpr uint32_t k_frames_in_flight = 3u;
    }

    CpuIndirectRenderer::~CpuIndirectRenderer()
    {
        if (m_context_p) { m_context_p->getDevice().waitIdle(); }
    }

    void CpuIndirectRenderer::create(
        render::VulkanContext * context,
        BenchmarkScene * scene,
        std::pair<uint32_t, uint32_t> max_extent,
        ecs::Registry & /*registry*/)
    {
        if (m_created) { return; }
        m_context_p = context;
        m_scene_p   = scene;

        m_frame_resources.resize(k_frames_in_flight);
        m_frame_has_history.assign(k_frames_in_flight, false);

        auto device = context->getDevice();
        auto & ds_manager = context->getDescriptorSetManager();

        // ---------- graphics pipeline ----------
        // 用 benchmark 私有 shader（fork 自 vertex_buffer_test.*）：
        //   - benchmark_indirect.vert：内容等价 vertex_buffer_test.vert
        //   - benchmark_indirect.frag：修复 MaterialTextureIds 5 槽错位（含 occlusion）+ emissive 直累加
        // 与 main_example 完全隔离，便于独立演化。
        auto graphics_program = std::make_shared<VulkanShaderProgram>();
        graphics_program
            ->addShaderFromGlslFile(ShaderTypeFlagBits::eVertex,   "assets/shaders/benchmark_indirect.vert")
            .addShaderFromGlslFile (ShaderTypeFlagBits::eFragment, "assets/shaders/benchmark_indirect.frag")
            .specifyDescriptorSetLayout(scene->getPerViewDescriptorSetLayout())
            .specifyDescriptorSetLayout(ds_manager.getBindlessBufferSet().getLayout())
            .specifyDescriptorSetLayout(ds_manager.getBindlessTextureSet().getLayout())
            .link(device);
        GraphicPipelineCreateInfo graphic_info;
        graphic_info.setShaderProgram(graphics_program)
            .setDepthAttachmentFormat(vk::Format::eD32Sfloat)
            .setRasterizationSamples(vk::SampleCountFlagBits::e4)
            .addColorAttachmentFormat(vk::Format::eR16G16B16A16Sfloat);
        // 注意：CPU 间接路径不需要 eIndirectBindableEXT（DGC 才需要）；
        // 但 vkCmdDrawIndirectCount 本身不需要 pipeline 标记，普通 graphics pipeline 即可。
        m_graphics_pipeline.create(m_context_p, graphic_info);

        // ---------- frame resources ----------
        auto [max_w, max_h] = max_extent;
        for (auto & fr : m_frame_resources) {
            fr.graphics_cmd.create(m_context_p, vk::QueueFlagBits::eGraphics);
            fr.transfer_cmd.create(m_context_p, vk::QueueFlagBits::eTransfer);

            VulkanFramebufferObjectCreateInfo fbo_info;
            fbo_info.setMaxExtent({max_w, max_h})
                .addColorFormat(vk::Format::eR16G16B16A16Sfloat)
                .setSampleCount(vk::SampleCountFlagBits::e4)
                .setDepthStencilFormat(vk::Format::eD32Sfloat)
                .setResolveMode(vk::ResolveModeFlagBits::eAverage);
            fr.fbo.create(m_context_p, fbo_info);
        }

        m_timestamp_pool.create(context, k_frames_in_flight);
        m_created = true;
        lcf_log_info("CpuIndirectRenderer created");
    }

    uint32_t CpuIndirectRenderer::cullOnCpu()
    {
        // 与 cull.comp 等价的 host 端实现：对每个 (mesh m, instance i)
        // 用 world_center / world_radius 做 6 平面 frustum test。
        //
        // 关键约束：vertex shader 用 `instance_id = visible_instance_ids[gl_InstanceIndex]`
        // 索引可见列表，而 `gl_InstanceIndex = firstInstance + InstanceID`。
        // 因此可见 instance_id 必须写入 `visible_instance_ids[first_instance + slot_within_mesh]`，
        // 这与 cull.comp 第 73 行的 `visible_instance_ids[first_instance_id + visible_offset]` 完全等价。
        //
        // 流程：
        //   1) resetDrawMetaToOriginal — 把 instance_count 还原为原始值，避免逐帧污染；
        //   2) resizeVisibleInstancesBuffer — 把 host shadow 容量 = m_total_instance_count 并清零；
        //   3) 对每个 mesh：从原始 first_instance / count 出发遍历，按 slot 写入可见 id。

        m_scene_p->resetDrawMetaToOriginal();
        m_scene_p->resizeVisibleInstancesBuffer();

        const auto & frustum   = m_scene_p->getCameraDataShadow().m_frustum;
        const auto draw_metas  = m_scene_p->getDrawMetaInfos();        // 已被 reset 为原始值
        const auto instances   = m_scene_p->getInstanceDataList();
        const auto bounds      = m_scene_p->getBoundingSpheresList();

        uint32_t total_visible = 0;
        for (uint32_t mi = 0; mi < draw_metas.size(); ++mi) {
            const auto & meta = draw_metas[mi];
            const auto & local_sphere = bounds[meta.m_object_id];
            const uint32_t count = meta.m_instance_count;
            const uint32_t first = meta.m_first_instance;

            uint32_t visible_for_this_mesh = 0;
            for (uint32_t i = 0; i < count; ++i) {
                const uint32_t instance_id = first + i;
                const auto & xform = instances[instance_id].m_transform;
                Vector4D<float> local_center4{
                    local_sphere.getCenter().getX(),
                    local_sphere.getCenter().getY(),
                    local_sphere.getCenter().getZ(),
                    1.0f};
                Vector4D<float> world_center4 = xform * local_center4;
                Vector3D<float> world_center{
                    world_center4.getX(), world_center4.getY(), world_center4.getZ()};
                const auto col0 = xform.getColumn(0);
                const float scale_approx = std::sqrt(
                    col0.getX() * col0.getX() +
                    col0.getY() * col0.getY() +
                    col0.getZ() * col0.getZ());
                const float world_radius = local_sphere.getRadius() * scale_approx;

                if (frustum.isSphereInside(world_center, world_radius)) {
                    // 按槽位写：first + visible_for_this_mesh，与 cull.comp 完全等价。
                    m_scene_p->writeVisibleInstanceAt(first + visible_for_this_mesh, instance_id);
                    ++visible_for_this_mesh;
                }
            }
            // 改写 draw_meta_infos[mi].instance_count 为剔除后的可见数。
            m_scene_p->overrideDrawMetaInstanceCount(mi, visible_for_this_mesh);
            total_visible += visible_for_this_mesh;
        }
        return total_visible;
    }

    FrameMetrics CpuIndirectRenderer::render(
        const ecs::Entity & camera, const ecs::Entity & render_target)
    {
        FrameMetrics metrics;
        if (not m_created) { return metrics; }

        auto & frame  = m_frame_resources[m_current_frame_index];
        auto & cmd    = frame.graphics_cmd;
        cmd.waitUntilAvailable();

        auto target_wp = render_target.getComponent<std::weak_ptr<VulkanSwapchain>>();
        if (target_wp.expired()) { return metrics; }
        const auto target_sp = target_wp.lock();
        if (not target_sp->prepareForRender()) { return metrics; }

        // 上一轮同帧位 GPU 时间戳回读。
        if (m_frame_has_history[m_current_frame_index]) {
            const auto ts = m_timestamp_pool.readBackFrame(m_current_frame_index);
            metrics.m2_gpu_frame_ms = ts.m_gpu_frame_ms;
            // CPU 路径无 GPU compute 阶段，m4 始终 = 0。
        }

        const auto cpu_t0 = std::chrono::steady_clock::now();

        const auto [width, height] = target_sp->getExtent();
        vk::Viewport viewport;
        viewport.setX(0.0f).setY(static_cast<float>(height))
                .setWidth (static_cast<float>(width))
                .setHeight(-static_cast<float>(height))
                .setMinDepth(0.0f).setMaxDepth(1.0f);
        vk::Rect2D scissor;
        scissor.setOffset({0, 0}).setExtent({width, height});

        // 1) 先更新 camera shadow → 然后 CPU 端做剔除（写入 visible / 重写 draw_meta instance_count）。
        m_scene_p->updateCameraShadow(camera, {width, height});
        this->cullOnCpu();

        // 2) transfer cmd：把 host shadow（含剔除后 draw_meta_infos 与 visible_instances）commit。
        auto & transfer_cmd = frame.transfer_cmd;
        transfer_cmd.begin(vk::CommandBufferBeginInfo{});
        m_scene_p->prepareFrameDataTransfer(transfer_cmd, camera, {width, height});
        transfer_cmd.end();
        const auto transfer_done = transfer_cmd.submit();

        // 3) graphics cmd：viewport / set bind / drawIndirectCount × 1。
        cmd.begin(vk::CommandBufferBeginInfo{});
        m_timestamp_pool.resetFrame(cmd, m_current_frame_index);
        m_timestamp_pool.writeGraphicsBegin(cmd, m_current_frame_index,
                                            vk::PipelineStageFlagBits2::eTopOfPipe);
        cmd.setViewport(0, viewport);
        cmd.setScissor (0, scissor);

        frame.fbo.beginRendering(cmd);

        // 先画天空盒（与 NaiveCpuRenderer 一致；与主网格共享 fbo / depth）。
        const auto & skybox_pipeline = m_scene_p->getSkyboxPipeline();
        cmd.bindPipeline(skybox_pipeline);
        cmd.bindDescriptorSet(skybox_pipeline, m_scene_p->getPerViewDescriptorSet());
        cmd.bindDescriptorSet(skybox_pipeline,
                              m_context_p->getDescriptorSetManager().getBindlessBufferSet());
        cmd.bindDescriptorSet(skybox_pipeline,
                              m_context_p->getDescriptorSetManager().getBindlessTextureSet());
        cmd.draw(36u, 1u, 0u, 0u);

        cmd.bindPipeline(m_graphics_pipeline);
        cmd.bindDescriptorSet(m_graphics_pipeline, m_scene_p->getPerViewDescriptorSet());
        cmd.bindDescriptorSet(m_graphics_pipeline,
                              m_context_p->getDescriptorSetManager().getBindlessBufferSet());
        cmd.bindDescriptorSet(m_graphics_pipeline,
                              m_context_p->getDescriptorSetManager().getBindlessTextureSet());

        // drawIndirectCount(buffer, offset, countBuffer, countOffset, maxDrawCount, stride)
        // - buffer: draw_meta SSBO，offset = sizeof(uint32_t) 跳过 head 的 draw_count 字段
        // - countBuffer: 同 buffer，countOffset = 0（head 即 draw_count）
        // - stride: sizeof(DrawMetaInfo) 与 GLSL std430 对齐一致（20 字节）
        const auto & draw_meta_ssbo =
            m_scene_p->getBindlessBufferGroup()
                [std::to_underlying(vkenums::BindlessBufferBinding::eDrawMetaInfos)];
        const auto draw_count = static_cast<uint32_t>(m_scene_p->getDrawMetaInfos().size());
        cmd.drawIndirectCount(
            draw_meta_ssbo.getHandle(), sizeof(uint32_t),
            draw_meta_ssbo.getHandle(), 0u,
            draw_count, sizeof(DrawMetaInfo));

        frame.fbo.endRendering(cmd);

        // MSAA 解析 → swapchain image。
        auto & target_resources = target_sp->getCurrentFrameResources();
        auto & target_image = *target_resources.getImageSharedPointer();
        auto msaa_attachment = frame.fbo.getMSAAResolveAttachment();
        if (msaa_attachment) {
            VulkanAttachment dst(target_image);
            const auto [tw, th, tz] = target_image.getExtent();
            msaa_attachment->blitTo(cmd, dst, vk::Filter::eLinear,
                {{0, 0, 0}, {static_cast<int32_t>(width), static_cast<int32_t>(height), 1}},
                {{0, 0, 0}, {static_cast<int32_t>(tw),   static_cast<int32_t>(th),   1}});
        }
        target_image.transitLayout(cmd, vk::ImageLayout::ePresentSrcKHR);

        m_timestamp_pool.writeGraphicsEnd(cmd, m_current_frame_index,
                                          vk::PipelineStageFlagBits2::eBottomOfPipe);

        cmd.end();

        vk::SemaphoreSubmitInfo wait_acquired;
        wait_acquired.setSemaphore(target_resources.getTargetAvailableSemaphore())
                     .setStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput);
        cmd.addWaitSubmitInfo(wait_acquired)
           .addWaitSubmitInfo(transfer_done)
           .addSignalSubmitInfo(target_resources.getPresentReadySemaphore());
        cmd.submit();

        target_sp->finishRender();

        const auto cpu_t1 = std::chrono::steady_clock::now();
        metrics.m1_cpu_submit_ms =
            std::chrono::duration<double, std::milli>(cpu_t1 - cpu_t0).count();
        // M3：host 端 vkCmdDrawIndirectCount 一次提交。chap05 §5.4.2 规定 host 视角统计。
        metrics.m3_draw_calls = 1u;

        m_frame_has_history[m_current_frame_index] = true;
        ++m_current_frame_index %= m_frame_resources.size();
        return metrics;
    }

}  // namespace lcf::benchmark
