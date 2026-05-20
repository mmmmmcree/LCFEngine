#pragma once

// NaiveCpuRenderer：朴素 CPU 绘制路径。
//
// 与其它两路径的差异：
//   - 不创建 compute pipeline / DGC / 间接绘制
//   - CPU 端逐 mesh 逐 visible instance 调用 vkCmdDraw（一次只画 1 个 instance）
//   - 着色器使用 push constant 传 object_id；instance 用 vkCmdDraw 的 firstInstance
//     参数传入，shader 端 gl_InstanceIndex 直接当 instance_id 索引 instance_data[]
//   - 不需要 visible_instance_ids SSBO（由 vkCmdDraw 的 firstInstance 替代）
//   - M3 = 实际 vkCmdDraw 调用次数（场景规模放大时线性增长）

#include "BenchmarkScene.h"
#include "GpuTimestampQueryPool.h"
#include "IBenchmarkRenderer.h"

#include <cstdint>
#include <utility>
#include <vector>

#include <vulkan/vulkan.hpp>

#include "Vulkan/VulkanCommandBufferObject.h"
#include "Vulkan/VulkanFramebufferObject.h"
#include "Vulkan/VulkanPipeline.h"

namespace lcf::benchmark {

    class NaiveCpuRenderer final : public IBenchmarkRenderer
    {
    public:
        NaiveCpuRenderer() = default;
        ~NaiveCpuRenderer() override;
        NaiveCpuRenderer(const NaiveCpuRenderer &) = delete;
        NaiveCpuRenderer & operator=(const NaiveCpuRenderer &) = delete;

        void create(render::VulkanContext * context,
                    BenchmarkScene * scene,
                    std::pair<uint32_t, uint32_t> max_extent,
                    ecs::Registry & registry) override;

        FrameMetrics render(const ecs::Entity & camera,
                            const ecs::Entity & render_target) override;

        ePath getPath() const noexcept override { return ePath::eCpuDrivenNaive; }

        // 仅接受 eClean / eLegacy；其它 mode 静默忽略。
        // - eClean：现状（per-mesh push_constants + per-instance vkCmdDraw）
        // - eLegacy：per-instance rebind 2 ds（bindless_buffer + bindless_texture）
        //            + 每 m_pipeline_switch_period 实例切一次 PSO 模拟 driver 重 setup
        void setEmulationMode(eEmulationMode mode) override;

        // 仅 eLegacy 用：配置每多少 instance 切一次 PSO。默认 64。
        void setPipelineSwitchPeriod(uint32_t period) noexcept { m_pipeline_switch_period = (period == 0u ? 1u : period); }

    private:
        struct FrameResources
        {
            render::VulkanCommandBufferObject  graphics_cmd;
            render::VulkanCommandBufferObject  transfer_cmd;
            render::VulkanFramebufferObject    fbo;
        };

        // 与 cull.comp 等价的 host 端剔除；返回 (mesh_visible_instances) 列表，
        // 每个元素是该 mesh 的可见 instance_id 数组（用于逐 vkCmdDraw 提交）。
        // 同时改写 m_scene 的 draw_meta_infos[i].instance_count 为剔除后值（公平起见）。
        std::vector<std::vector<uint32_t>> cullOnCpu();

    private:
        render::VulkanContext * m_context_p = nullptr;
        BenchmarkScene        * m_scene_p   = nullptr;

        std::vector<FrameResources> m_frame_resources;
        uint32_t                    m_current_frame_index = 0;

        render::VulkanPipeline m_graphics_pipeline;
        // 第二个 PSO，仅 eLegacy 模式下用于"每 N 实例切一次 pipeline"模拟驱动 state switch。
        // 与 m_graphics_pipeline 同 program / 同 layout（保证 ds 二进制兼容），
        // 仅 cullMode 不同（a=eBack, b=eFront；封闭模型 trackball 视角下视觉无差）。
        render::VulkanPipeline m_graphics_pipeline_b;
        GpuTimestampQueryPool  m_timestamp_pool;
        std::vector<bool>      m_frame_has_history;

        // 模拟模式状态：默认 eClean 保持现状。
        eEmulationMode m_mode = eEmulationMode::eClean;
        uint32_t       m_pipeline_switch_period = 64u;

        bool m_created = false;
    };

}  // namespace lcf::benchmark
