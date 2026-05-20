// NaiveCpuRenderer 实现。
//
// 朴素 CPU 路径：CPU 端遍历 (mesh × instance) 做视锥剔除，对每个可见 instance
// 单独发起 vkCmdDraw(index_count, 1, 0, instance_id)。其中 instance_id 通过
// firstInstance 参数传入，shader 内 gl_InstanceIndex 直接得到该值。同时通过
// push constant 传 object_id，避免依赖 gl_DrawID（非 indirect 调用恒为 0）。

#include "NaiveCpuRenderer.h"

#include <chrono>
#include <utility>

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

    NaiveCpuRenderer::~NaiveCpuRenderer()
    {
        if (m_context_p) { m_context_p->getDevice().waitIdle(); }
    }

    void NaiveCpuRenderer::setEmulationMode(eEmulationMode mode)
    {
        // 只接 eClean / eLegacy；其它 mode 静默忽略（避免 RendererSwitcher 误传 single/batched）。
        if (mode != eEmulationMode::eClean && mode != eEmulationMode::eLegacy) {
            lcf_log_warn("NaiveCpuRenderer: ignore unsupported mode={}", to_csv_name(mode));
            return;
        }
        if (m_mode != mode) {
            lcf_log_info("NaiveCpuRenderer: switch mode -> {}", to_csv_name(mode));
            m_mode = mode;
        }
    }

    void NaiveCpuRenderer::create(
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
        // 使用本路径专用 shader（push constant 传 object_id）；其它两路径仍用 vertex_buffer_test.*
        auto graphics_program = std::make_shared<VulkanShaderProgram>();
        graphics_program
            ->addShaderFromGlslFile(ShaderTypeFlagBits::eVertex,   "assets/shaders/benchmark_naive.vert")
            .addShaderFromGlslFile (ShaderTypeFlagBits::eFragment, "assets/shaders/benchmark_naive.frag")
            .specifyDescriptorSetLayout(scene->getPerViewDescriptorSetLayout())
            .specifyDescriptorSetLayout(ds_manager.getBindlessBufferSet().getLayout())
            .specifyDescriptorSetLayout(ds_manager.getBindlessTextureSet().getLayout())
            .link(device);

        GraphicPipelineCreateInfo graphic_info;
        graphic_info.setShaderProgram(graphics_program)
            .setDepthAttachmentFormat(vk::Format::eD32Sfloat)
            .setRasterizationSamples(vk::SampleCountFlagBits::e4)
            .addColorAttachmentFormat(vk::Format::eR16G16B16A16Sfloat)
            .setCullMode(vk::CullModeFlagBits::eBack);
        m_graphics_pipeline.create(m_context_p, graphic_info);

        // pipeline_b：与 a 同 program / 同 layout，仅 cullMode 反向。
        // 对封闭 mesh（FlightHelmet / DamagedHelmet）trackball 视角下视觉无差；
        // 用于 eLegacy 模式每 N 实例 toggle，模拟驱动 PSO state switch。
        GraphicPipelineCreateInfo graphic_info_b = graphic_info;
        graphic_info_b.setCullMode(vk::CullModeFlagBits::eFront);
        m_graphics_pipeline_b.create(m_context_p, graphic_info_b);

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
        lcf_log_info("NaiveCpuRenderer created");
    }

    std::vector<std::vector<uint32_t>> NaiveCpuRenderer::cullOnCpu()
    {
        // 每帧 reset draw_meta 防止跨帧污染；本路径同样要 reset，原因与 CpuIndirect 同。
        m_scene_p->resetDrawMetaToOriginal();

        const auto & frustum   = m_scene_p->getCameraDataShadow().m_frustum;
        const auto draw_metas  = m_scene_p->getDrawMetaInfos();
        const auto instances   = m_scene_p->getInstanceDataList();
        const auto bounds      = m_scene_p->getBoundingSpheresList();

        std::vector<std::vector<uint32_t>> per_mesh_visible(draw_metas.size());

        for (uint32_t mi = 0; mi < draw_metas.size(); ++mi) {
            const auto & meta = draw_metas[mi];
            const auto & local_sphere = bounds[meta.m_object_id];
            const uint32_t count = meta.m_instance_count;
            const uint32_t first = meta.m_first_instance;

            auto & visible_for_this_mesh = per_mesh_visible[mi];
            visible_for_this_mesh.reserve(count / 4 + 4);

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
                    visible_for_this_mesh.emplace_back(instance_id);
                }
            }
            // 公平起见：把 draw_meta_infos[mi].instance_count 改为剔除后的值，
            // 其它仍是 host shadow 状态（虽然朴素路径不上传 visible_ssbo，但让 shadow
            // 与三路径"剔除位置不同、可见对象集合相同"的语义一致）。
            m_scene_p->overrideDrawMetaInstanceCount(
                mi, static_cast<uint32_t>(visible_for_this_mesh.size()));
        }
        return per_mesh_visible;
    }

    FrameMetrics NaiveCpuRenderer::render(
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

        if (m_frame_has_history[m_current_frame_index]) {
            const auto ts = m_timestamp_pool.readBackFrame(m_current_frame_index);
            metrics.m2_gpu_frame_ms = ts.m_gpu_frame_ms;
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

        // 1) 更新 camera shadow → 2) CPU 视锥剔除得到逐 mesh 的可见 id 列表。
        // 用 chrono 单独包住 cull，归到 M4_cull_ms；M1 不再吃这段（与 GpuDriven 对称）。
        m_scene_p->updateCameraShadow(camera, {width, height});
        const auto cull_t0 = std::chrono::steady_clock::now();
        auto per_mesh_visible = this->cullOnCpu();
        const auto cull_t1 = std::chrono::steady_clock::now();
        const double cull_ms =
            std::chrono::duration<double, std::milli>(cull_t1 - cull_t0).count();

        // 3) transfer cmd：上传 PerView UBO + 5 路 SSBO（draw_meta 已 reset & 重写）。
        // 朴素路径不上传 visible_instances（shader 不读它）；BenchmarkScene 已在 path
        // 切换时清空 visible host shadow，prepareFrameDataTransfer 不会写 visible_ssbo。
        auto & transfer_cmd = frame.transfer_cmd;
        transfer_cmd.begin(vk::CommandBufferBeginInfo{});
        m_scene_p->prepareFrameDataTransfer(transfer_cmd, camera, {width, height});
        transfer_cmd.end();
        const auto transfer_done = transfer_cmd.submit();

        // 4) graphics cmd：逐 mesh 遍历可见 instance，每个发一次 vkCmdDraw。
        cmd.begin(vk::CommandBufferBeginInfo{});
        m_timestamp_pool.resetFrame(cmd, m_current_frame_index);
        m_timestamp_pool.writeGraphicsBegin(cmd, m_current_frame_index,
                                            vk::PipelineStageFlagBits2::eTopOfPipe);
        cmd.setViewport(0, viewport);
        cmd.setScissor (0, scissor);

        frame.fbo.beginRendering(cmd);

        // 先画天空盒：bind skybox pipeline + 3 个 ds + draw 36 顶点。
        // skybox.vert 用 .xyww trick 把 z 推到远平面，与主网格深度共存。
        const auto & skybox_pipeline = m_scene_p->getSkyboxPipeline();
        cmd.bindPipeline(skybox_pipeline);
        cmd.bindDescriptorSet(skybox_pipeline, m_scene_p->getPerViewDescriptorSet());
        cmd.bindDescriptorSet(skybox_pipeline,
                              m_context_p->getDescriptorSetManager().getBindlessBufferSet());
        cmd.bindDescriptorSet(skybox_pipeline,
                              m_context_p->getDescriptorSetManager().getBindlessTextureSet());
        cmd.draw(36u, 1u, 0u, 0u);

        // 主管线 + 主 ds 绑定（clean / legacy 共用起手）。
        cmd.bindPipeline(m_graphics_pipeline);
        cmd.bindDescriptorSet(m_graphics_pipeline, m_scene_p->getPerViewDescriptorSet());
        cmd.bindDescriptorSet(m_graphics_pipeline,
                              m_context_p->getDescriptorSetManager().getBindlessBufferSet());
        cmd.bindDescriptorSet(m_graphics_pipeline,
                              m_context_p->getDescriptorSetManager().getBindlessTextureSet());

        const auto draw_metas = m_scene_p->getDrawMetaInfos();
        const auto & meshes_packs = m_scene_p->getMeshPacks();

        // 把 mesh_id（draw_meta_infos 索引）映射到 (mesh_pack, mesh_in_pack) — 顺序保留。
        // BenchmarkScene::setSceneScale 内是按 mesh_packs 的扁平顺序生成 draw_meta_infos 的。
        uint32_t total_draw_calls = 0;
        if (m_mode == eEmulationMode::eClean) {
            // ===== Clean 路径（理想化 bindless）：per-mesh push_const + per-instance vkCmdDraw =====
            uint32_t mi = 0;
            for (const auto & pack : meshes_packs) {
                for (const auto & mesh : pack.meshes) {
                    const auto & visible_ids = per_mesh_visible[mi];
                    if (not visible_ids.empty()) {
                        const uint32_t object_id = draw_metas[mi].m_object_id;
                        auto & shader_program = *m_graphics_pipeline.getShaderProgram();
                        shader_program.setPushConstantData(
                            vk::ShaderStageFlagBits::eVertex,
                            {static_cast<const void *>(&object_id)});
                        shader_program.bindPushConstants(cmd);

                        const uint32_t index_count = mesh.getIndexCount();
                        for (uint32_t instance_id : visible_ids) {
                            cmd.draw(index_count, 1u, 0u, instance_id);
                            ++total_draw_calls;
                        }
                    }
                    ++mi;
                }
            }
        } else {
            // ===== Legacy 路径（"工业悲观 baseline"模拟）=====
            //   - 每 instance：rebind 2 个 ds（bindless_buffer + bindless_texture）
            //     即使 ds 内容没变，host 仍发出 vkCmdBindDescriptorSets，模拟驱动 dirty validation
            //   - 每 m_pipeline_switch_period 个 instance：toggle PSO（a ↔ b），
            //     切换后强制 rebind 全部 3 个 ds + push_const，模拟"PSO 切换让 driver state 全部失效"
            //   - 注意：active_pipeline 引用要保持稳定，setPushConstantData 走 active 的 program
            auto & ds_manager     = m_context_p->getDescriptorSetManager();
            const auto & per_view_ds    = m_scene_p->getPerViewDescriptorSet();
            auto & bindless_buf_ds = ds_manager.getBindlessBufferSet();
            auto & bindless_tex_ds = ds_manager.getBindlessTextureSet();

            const render::VulkanPipeline * active_pipeline = &m_graphics_pipeline;
            uint32_t draw_counter = 0;

            uint32_t mi = 0;
            for (const auto & pack : meshes_packs) {
                for (const auto & mesh : pack.meshes) {
                    const auto & visible_ids = per_mesh_visible[mi];
                    if (visible_ids.empty()) { ++mi; continue; }

                    const uint32_t object_id   = draw_metas[mi].m_object_id;
                    const uint32_t index_count = mesh.getIndexCount();

                    // 进入新 mesh：发 push_const（用当前 active pipeline 的 program）。
                    {
                        auto & program = *active_pipeline->getShaderProgram();
                        program.setPushConstantData(
                            vk::ShaderStageFlagBits::eVertex,
                            {static_cast<const void *>(&object_id)});
                        program.bindPushConstants(cmd);
                    }

                    for (uint32_t instance_id : visible_ids) {
                        // 周期性 PSO toggle（draw_counter > 0 才切，避免首帧无意义 rebind）
                        if (draw_counter > 0u && (draw_counter % m_pipeline_switch_period == 0u)) {
                            active_pipeline = (active_pipeline == &m_graphics_pipeline)
                                                  ? &m_graphics_pipeline_b
                                                  : &m_graphics_pipeline;
                            cmd.bindPipeline(*active_pipeline);
                            // PSO 切换后强制 rebind 全部 3 ds + push_const（模拟最坏路径）
                            cmd.bindDescriptorSet(*active_pipeline, per_view_ds);
                            cmd.bindDescriptorSet(*active_pipeline, bindless_buf_ds);
                            cmd.bindDescriptorSet(*active_pipeline, bindless_tex_ds);
                            auto & program = *active_pipeline->getShaderProgram();
                            program.setPushConstantData(
                                vk::ShaderStageFlagBits::eVertex,
                                {static_cast<const void *>(&object_id)});
                            program.bindPushConstants(cmd);
                        }
                        // 每实例固定 rebind 2 ds（即使内容相同，host 调用栈仍发出）。
                        cmd.bindDescriptorSet(*active_pipeline, bindless_buf_ds);
                        cmd.bindDescriptorSet(*active_pipeline, bindless_tex_ds);

                        cmd.draw(index_count, 1u, 0u, instance_id);
                        ++total_draw_calls;
                        ++draw_counter;
                    }
                    ++mi;
                }
            }
        }

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
        // M1 = 总 CPU 段 - cull 段；M4 = cull 段（host chrono）
        metrics.m1_cpu_submit_ms =
            std::chrono::duration<double, std::milli>(cpu_t1 - cpu_t0).count() - cull_ms;
        metrics.m4_cull_ms       = cull_ms;
        metrics.m3_draw_calls    = total_draw_calls;

        m_frame_has_history[m_current_frame_index] = true;
        ++m_current_frame_index %= m_frame_resources.size();
        return metrics;
    }

}  // namespace lcf::benchmark
