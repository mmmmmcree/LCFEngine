// GpuDrivenRenderer 实现。
//
// 黄金参考：RenderEngine/src/Vulkan/VulkanRenderer.cpp（用户认定零 bug 的实现）。
// 本类做以下精简：
//   - 不创建 skybox / sphere_to_cube 子流程；
//   - DGC sequence_count 从 2 缩减为 1（仅 mesh）；
//   - 模型 / 材质 / 纹理 / UBO / SSBO / bindless DS 全部由 BenchmarkScene 提供；
//   - 增加 timestamp 查询与 chrono 计时输出 FrameMetrics。

#include "GpuDrivenRenderer.h"

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
#include "bytes.h"
#include "common/glsl_type_traits.h"
#include "ecs/Entity.h"
#include "ecs/Registry.h"
#include "log.h"
#include "tracy_profiling.h"
#include "VkDebugLabel.h"

using lcf::ShaderTypeFlagBits;
using lcf::render::ComputePipelineCreateInfo;
using lcf::render::GPUBufferPattern;
using lcf::render::GPUBufferUsage;
using lcf::render::GraphicPipelineCreateInfo;
using lcf::render::VulkanAttachment;
using lcf::render::VulkanFramebufferObjectCreateInfo;
using lcf::render::VulkanImageObject;
using lcf::render::VulkanShaderProgram;
using lcf::render::VulkanSwapchain;
namespace vkenums = lcf::render::vkenums;

namespace lcf::benchmark {

    namespace {
        constexpr uint32_t k_frames_in_flight = 3u;
        constexpr uint32_t k_sequence_count   = 1u;     // mesh only
        constexpr uint32_t k_max_mesh_draws   = 256u;   // 与 VulkanRenderer.cpp 行 410 对位（128→256 留余量）
    }

    GpuDrivenRenderer::~GpuDrivenRenderer()
    {
        if (m_context_p) { m_context_p->getDevice().waitIdle(); }
    }

    void GpuDrivenRenderer::setEmulationMode(eEmulationMode mode)
    {
        if (mode != eEmulationMode::eGpuDriven && mode != eEmulationMode::eGpuIndirectCount) {
            lcf_log_warn("GpuDrivenRenderer: ignore unsupported mode={}", to_csv_name(mode));
            return;
        }
        if (m_mode != mode) {
            lcf_log_info("GpuDrivenRenderer: switch mode -> {}", to_csv_name(mode));
            m_mode = mode;
        }
    }

    void GpuDrivenRenderer::create(
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

        this->initPipelines();
        this->initFrameResources(max_extent);
        this->initDgc();

        m_timestamp_pool.create(context, k_frames_in_flight);

        m_created = true;
        lcf_log_info("GpuDrivenRenderer created");
    }

    void GpuDrivenRenderer::initPipelines()
    {
        auto & ds_manager = m_context_p->getDescriptorSetManager();
        auto device       = m_context_p->getDevice();
        const auto & per_view_layout = m_scene_p->getPerViewDescriptorSetLayout();

        // ---------- 1) compute pipeline (benchmark_cull.comp) ----------
        // 私有版本，加了 push_const pc_disable_cull 用于 ABL-CULL 消融。
        auto compute_program = std::make_shared<VulkanShaderProgram>();
        compute_program->addShaderFromGlslFile(ShaderTypeFlagBits::eCompute, "assets/shaders/benchmark_cull.comp")
            .specifyDescriptorSetLayout(per_view_layout)
            .specifyDescriptorSetLayout(ds_manager.getBindlessBufferSet().getLayout())
            .link(device);
        ComputePipelineCreateInfo compute_info(compute_program);
        m_compute_pipeline.create(m_context_p, compute_info);

        // ---------- 2) graphics pipeline (benchmark_indirect.*) ----------
        // 切到 benchmark 私有 shader：与 CpuIndirectRenderer 共用，含 MaterialTextureIds 5 槽修复。
        auto graphics_program = std::make_shared<VulkanShaderProgram>();
        graphics_program->addShaderFromGlslFile(ShaderTypeFlagBits::eVertex,   "assets/shaders/benchmark_indirect.vert")
            .addShaderFromGlslFile           (ShaderTypeFlagBits::eFragment, "assets/shaders/benchmark_indirect.frag")
            .specifyDescriptorSetLayout(per_view_layout)
            .specifyDescriptorSetLayout(ds_manager.getBindlessBufferSet().getLayout())
            .specifyDescriptorSetLayout(ds_manager.getBindlessTextureSet().getLayout())
            .link(device);
        GraphicPipelineCreateInfo graphic_info;
        graphic_info.setShaderProgram(graphics_program)
            .setDepthAttachmentFormat(vk::Format::eD32Sfloat)
            .setRasterizationSamples(vk::SampleCountFlagBits::e4)
            .addColorAttachmentFormat(vk::Format::eR16G16B16A16Sfloat)
            .addPipelineCreateFlags2(vk::PipelineCreateFlagBits2::eIndirectBindableEXT);  // DGC 必需
        m_graphics_pipeline.create(m_context_p, graphic_info);
    }

    void GpuDrivenRenderer::initFrameResources(std::pair<uint32_t, uint32_t> max_extent)
    {
        auto [max_w, max_h] = max_extent;
        for (auto & fr : m_frame_resources) {
            fr.graphics_cmd.create(m_context_p, vk::QueueFlagBits::eGraphics);
            fr.transfer_cmd.create(m_context_p, vk::QueueFlagBits::eTransfer);
            fr.compute_cmd .create(m_context_p, vk::QueueFlagBits::eCompute);

            VulkanFramebufferObjectCreateInfo fbo_info;
            fbo_info.setMaxExtent({max_w, max_h})
                .addColorFormat(vk::Format::eR16G16B16A16Sfloat)
                .setSampleCount(vk::SampleCountFlagBits::e4)
                .setDepthStencilFormat(vk::Format::eD32Sfloat)
                .setResolveMode(vk::ResolveModeFlagBits::eAverage);
            fr.fbo.create(m_context_p, fbo_info);
        }
    }

    void GpuDrivenRenderer::initDgc()
    {
        auto device = m_context_p->getDevice();

        // ---- IndirectExecutionSet：单 pipeline ----
        {
            std::vector<vk::WriteIndirectExecutionSetPipelineEXT> writes(1);
            writes[0].setIndex(0).setPipeline(m_graphics_pipeline);

            vk::IndirectExecutionSetPipelineInfoEXT pipeline_info;
            pipeline_info.setInitialPipeline(m_graphics_pipeline)
                         .setMaxPipelineCount(static_cast<uint32_t>(writes.size()));

            vk::IndirectExecutionSetCreateInfoEXT create_info;
            create_info.setType(vk::IndirectExecutionSetInfoTypeEXT::ePipelines)
                       .setInfo(&pipeline_info);
            m_indirect_execution_set = device.createIndirectExecutionSetEXTUnique(create_info);
            device.updateIndirectExecutionSetPipelineEXT(m_indirect_execution_set.get(), writes);
        }

        // ---- IndirectCommandsLayout：execution_set + draw_count 两个 token ----
        {
            vk::IndirectCommandsExecutionSetTokenEXT exec_token;
            exec_token.setType(vk::IndirectExecutionSetInfoTypeEXT::ePipelines)
                      .setShaderStages(vk::ShaderStageFlagBits::eVertex |
                                       vk::ShaderStageFlagBits::eFragment);

            std::array<vk::IndirectCommandsLayoutTokenEXT, 2> tokens;
            tokens[0].setType(vk::IndirectCommandsTokenTypeEXT::eExecutionSet)
                     .setOffset(offsetof(DrawSequence, pipeline_index))
                     .setData(vk::IndirectCommandsTokenDataEXT{&exec_token});
            tokens[1].setType(vk::IndirectCommandsTokenTypeEXT::eDrawCount)
                     .setOffset(offsetof(DrawSequence, draw_call));

            vk::IndirectCommandsLayoutCreateInfoEXT layout_info;
            layout_info.setFlags({})
                       .setShaderStages(vk::ShaderStageFlagBits::eVertex |
                                        vk::ShaderStageFlagBits::eFragment)
                       .setIndirectStride(sizeof(DrawSequence))
                       .setPipelineLayout(m_graphics_pipeline.getPipelineLayout())
                       .setTokens(tokens);
            m_indirect_commands_layout = device.createIndirectCommandsLayoutEXTUnique(layout_info);
        }

        // ---- sequence buffer + preprocess buffer ----
        // 使用 eStatic 与 VulkanRenderer.cpp 行 412 严格对位：每帧 addWriteSegment 后由
        // BufferWriter 走 staging+commit 路径，确保 GPU 仍在读旧 sequence 时 CPU 不会直写覆盖。
        m_sequence_buffer.setUsage(GPUBufferUsage::eIndirect)
                         .setPattern(GPUBufferPattern::eStatic)
                         .create(m_context_p, sizeof(DrawSequence) * k_sequence_count);

        vk::GeneratedCommandsMemoryRequirementsInfoEXT mem_req_info;
        mem_req_info.setIndirectExecutionSet(m_indirect_execution_set.get())
                    .setIndirectCommandsLayout(m_indirect_commands_layout.get())
                    .setMaxSequenceCount(k_sequence_count)
                    .setMaxDrawCount(k_max_mesh_draws);

        vk::MemoryRequirements2 mem_req;
        device.getGeneratedCommandsMemoryRequirementsEXT(&mem_req_info, &mem_req);
        const uint64_t preprocess_size = mem_req.memoryRequirements.size;

        m_preprocess_buffer.setUsage(GPUBufferUsage::ePreprocess)
                           .setPattern(GPUBufferPattern::eStatic)
                           .create(m_context_p, preprocess_size);
    }

    FrameMetrics GpuDrivenRenderer::render(
        const ecs::Entity & camera, const ecs::Entity & render_target)
    {
        LCF_TRACY_ZONE_N("GpuDriven::Frame");
        FrameMetrics metrics;
        if (not m_created) { return metrics; }

        auto device = m_context_p->getDevice();
        auto & frame  = m_frame_resources[m_current_frame_index];
        auto & cmd    = frame.graphics_cmd;

        // 等待该 frame slot 上一轮的 graphics submission 完成（命令缓冲对象内置 timeline）。
        cmd.waitUntilAvailable();

        // 拿到当前 swapchain 帧 image。
        auto target_wp = render_target.getComponent<std::weak_ptr<VulkanSwapchain>>();
        if (target_wp.expired()) { return metrics; }
        const auto target_sp = target_wp.lock();
        if (not target_sp->prepareForRender()) { return metrics; }

        // 上一轮同帧位的 GPU 时间戳（如果有历史）。
        if (m_frame_has_history[m_current_frame_index]) {
            const auto ts = m_timestamp_pool.readBackFrame(m_current_frame_index);
            metrics.m2_gpu_frame_ms = ts.m_gpu_frame_ms;
            metrics.m4_cull_ms      = ts.m_gpu_cull_ms;
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

        const uint32_t draw_count = static_cast<uint32_t>(m_scene_p->getDrawMetaInfos().size());

        // 提前装配 DrawSequence 写入请求（数据量小，与 scene 数据一并随 transfer cmd 上传）。
        DrawSequence sequence;
        sequence.pipeline_index = 0;
        const auto & draw_meta_ssbo =
            m_scene_p->getBindlessBufferGroup()
                [std::to_underlying(vkenums::BindlessBufferBinding::eDrawMetaInfos)];
        sequence.draw_call.setBufferAddress(draw_meta_ssbo.getDeviceAddress() + sizeof(uint32_t))
                          .setStride(sizeof(DrawMetaInfo))
                          .setCommandCount(draw_count);
        m_sequence_buffer.addWriteSegment({lcf::as_bytes_from_value(sequence), 0u});

        // ---- 1) transfer cmd：committed scene shadow + sequence buffer ----
        auto & transfer_cmd = frame.transfer_cmd;
        {
            LCF_TRACY_ZONE_N("GpuDriven::TransferRecord");
            transfer_cmd.begin(vk::CommandBufferBeginInfo{});
            m_scene_p->prepareFrameDataTransfer(transfer_cmd, camera, {width, height});
            m_sequence_buffer.commit(transfer_cmd);
            transfer_cmd.end();
        }
        const auto transfer_done = [&] {
            LCF_TRACY_ZONE_N("GpuDriven::TransferSubmit");
            return transfer_cmd.submit();
        }();

        // ---- 2) compute cmd：benchmark_cull.comp ----
        auto & compute_cmd = frame.compute_cmd;
        {
            LCF_TRACY_ZONE_N("GpuDriven::CullRecord");
            compute_cmd.begin(vk::CommandBufferBeginInfo{});
            m_timestamp_pool.resetFrame(compute_cmd, m_current_frame_index);
            m_timestamp_pool.writeComputeBegin(compute_cmd, m_current_frame_index,
                                               vk::PipelineStageFlagBits2::eTopOfPipe);

            compute_cmd.bindPipeline(m_compute_pipeline);
            compute_cmd.bindDescriptorSet(m_compute_pipeline, m_scene_p->getPerViewDescriptorSet());
            compute_cmd.bindDescriptorSet(m_compute_pipeline,
                                           m_context_p->getDescriptorSetManager().getBindlessBufferSet());

            // ABL-CULL 消融：push pc_disable_cull（0/1）。benchmark_cull.comp 内据此选择
            // bypass frustum_test 或正常 6 平面剔除。
            const uint32_t pc_disable = m_disable_cull ? 1u : 0u;
            auto & program = *m_compute_pipeline.getShaderProgram();
            program.setPushConstantData(
                vk::ShaderStageFlagBits::eCompute,
                {static_cast<const void *>(&pc_disable)});
            program.bindPushConstants(compute_cmd);

            // 包 vkCmdBeginDebugUtilsLabelEXT：nsys / RenderDoc timeline 上能看到
            // "GpuDriven::CullDispatch" 段的 GPU duration，作为 query pool M4_cull_ms 的双源认证。
            {
                VkDebugLabel _lbl(compute_cmd, "GpuDriven::CullDispatch");
                compute_cmd.dispatch(draw_count, 1u, 1u);
            }

            m_timestamp_pool.writeComputeEnd(compute_cmd, m_current_frame_index,
                                             vk::PipelineStageFlagBits2::eComputeShader);
            compute_cmd.end();
            compute_cmd.addWaitSubmitInfo(transfer_done);
        }
        const auto compute_done = [&] {
            LCF_TRACY_ZONE_N("GpuDriven::CullSubmit");
            return compute_cmd.submit();
        }();

        // ---- 3) graphics cmd：DGC executeGeneratedCommandsEXT ----
        cmd.begin(vk::CommandBufferBeginInfo{});

        m_timestamp_pool.writeGraphicsBegin(cmd, m_current_frame_index,
                                            vk::PipelineStageFlagBits2::eTopOfPipe);
        // M2 = graphics_end - graphics_begin。begin/end 使用 top/bottom of pipe，
        // 因此 M2 度量的是「graphics cmd 在 GPU 上从命令首条到最后一条 retired」的总时间，
        // 不区分 swapchain semaphore 等待时间。论文层将 M2 称为"GPU 帧时间"。

        cmd.setViewport(0, viewport);
        cmd.setScissor (0, scissor);

        frame.fbo.beginRendering(cmd);

        // 先画天空盒：在 DGC executeGeneratedCommandsEXT 之前 bind skybox pipeline + 3 个 ds 画一次。
        // 注意：cmd.executeGeneratedCommandsEXT 会切换到 indirect_execution_set 内的 pipeline，
        // 所以这里先画完 skybox 再调 DGC，互不干扰。
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

        // benchmark_indirect.vert 在 step5 加了 push_const pc_force_mesh_id 用于 CpuIndirect_legacy
        // 强制 mesh_id；GpuDriven 路径走 fallback（gl_DrawID = DGC sequence index 或 drawIndirectCount 累加值）。
        // 必须 push 一次 sentinel 0xFFFFFFFFu，否则 push_const 内容未定义 → mesh_id 错乱。
        {
            const uint32_t sentinel = 0xFFFFFFFFu;
            auto & program = *m_graphics_pipeline.getShaderProgram();
            program.setPushConstantData(
                vk::ShaderStageFlagBits::eVertex,
                {static_cast<const void *>(&sentinel)});
            program.bindPushConstants(cmd);
        }

        uint32_t m3_calls = 0u;
        if (m_mode == eEmulationMode::eGpuDriven) {
            // ===== 核心方案：DGC executeGeneratedCommandsEXT =====
            LCF_TRACY_ZONE_N("GpuDriven::DGCExecute");
            VkDebugLabel _lbl(cmd, "GpuDriven::DGCExecute");
            vk::GeneratedCommandsInfoEXT gen_info;
            gen_info.setShaderStages(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
                    .setIndirectExecutionSet(m_indirect_execution_set.get())
                    .setIndirectCommandsLayout(m_indirect_commands_layout.get())
                    .setIndirectAddress(m_sequence_buffer.getDeviceAddress())
                    // spec 要求 indirectAddressSize ≥ indirectStride * maxSequenceCount；
                    // 显式传 stride × count，避免依赖 getSizeInBytes() 的实现细节。
                    .setIndirectAddressSize(sizeof(DrawSequence) * k_sequence_count)
                    .setPreprocessAddress(m_preprocess_buffer.getDeviceAddress())
                    .setPreprocessSize(m_preprocess_buffer.getSizeInBytes())
                    .setMaxSequenceCount(k_sequence_count)
                    .setMaxDrawCount(k_max_mesh_draws)
                    .setSequenceCountAddress(0);
            cmd.executeGeneratedCommandsEXT(false, gen_info);
            m3_calls = 1u;
        } else {
            // ===== ABL-DGC 消融：GPU cull 仍开，但用 vkCmdDrawIndirectCount 替代 DGC =====
            // - draw_meta SSBO 已被 cull.comp 写过（instance_count = 可见数）
            // - drawIndirectCount(buffer, offset=4, countBuffer=同 buffer, countOffset=0,
            //                     maxDraw=draw_count, stride=20)
            // 与 CpuIndirect_single 唯一差别：cull 由 GPU 完成（M4 走 GPU timestamp）。
            LCF_TRACY_ZONE_N("GpuDriven::IndirectCount");
            VkDebugLabel _lbl(cmd, "GpuDriven::IndirectCount");
            cmd.drawIndirectCount(
                draw_meta_ssbo.getHandle(), sizeof(uint32_t),
                draw_meta_ssbo.getHandle(), 0u,
                draw_count, sizeof(DrawMetaInfo));
            m3_calls = 1u;  // host 端仍是 1 次 cmd
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
           .addWaitSubmitInfo(compute_done)
           .addSignalSubmitInfo(target_resources.getPresentReadySemaphore());
        {
            LCF_TRACY_ZONE_N("GpuDriven::GraphicsSubmit");
            cmd.submit();
        }

        {
            LCF_TRACY_ZONE_N("GpuDriven::Wait");  // M6
            target_sp->finishRender();
        }

        const auto cpu_t1 = std::chrono::steady_clock::now();
        metrics.m1_cpu_submit_ms =
            std::chrono::duration<double, std::milli>(cpu_t1 - cpu_t0).count();
        metrics.m3_draw_calls        = m3_calls;
        // m4_visible_instances 不在 GpuDriven 端回填（避免 GPU→CPU readback 污染 M1）；
        // 论文 CULL-RATE 表用同 scene 的 CpuIndirect 数据填充（剔除算法等价）。
        metrics.m4_visible_instances = 0u;

        m_frame_has_history[m_current_frame_index] = true;
        ++m_current_frame_index %= m_frame_resources.size();
        return metrics;
    }

}  // namespace lcf::benchmark
