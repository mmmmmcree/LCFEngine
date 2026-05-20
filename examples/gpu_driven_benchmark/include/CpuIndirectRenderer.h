#pragma once

// CpuIndirectRenderer：CPU 生成间接命令路径。
//
// 与 GpuDrivenRenderer 的差异：
//   - 不创建 compute pipeline / IndirectExecutionSet / IndirectCommandsLayout / preprocess buffer
//   - CPU 端遍历 (mesh, instance) 做视锥剔除，把可见 (mesh_id, instance_id) 写入
//     visible_instances SSBO，并改写 draw_meta_infos[i].instance_count 为剔除后的值
//   - 改用 vkCmdDrawIndirectCount (单次提交)，参数 buffer = draw_meta_infos SSBO
//     (跳过 head 的 4 字节 draw_count 字段)
//
// 与 SaschaWillems computecullandlod 思路相通：CPU 端预剔除 → indirect draw 单次提交。

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

    class CpuIndirectRenderer final : public IBenchmarkRenderer
    {
    public:
        CpuIndirectRenderer() = default;
        ~CpuIndirectRenderer() override;
        CpuIndirectRenderer(const CpuIndirectRenderer &) = delete;
        CpuIndirectRenderer & operator=(const CpuIndirectRenderer &) = delete;

        void create(render::VulkanContext * context,
                    BenchmarkScene * scene,
                    std::pair<uint32_t, uint32_t> max_extent,
                    ecs::Registry & registry) override;

        FrameMetrics render(const ecs::Entity & camera,
                            const ecs::Entity & render_target) override;

        ePath getPath() const noexcept override { return ePath::eCpuDrivenIndirect; }

        // 仅接受 eSingle / eLegacy；其它 mode 静默忽略。
        // - eSingle：现状（1 次 vkCmdDrawIndirectCount 全包；shader 端 mesh_id = gl_DrawID），
        //            享受 BDA+Bindless 基础设施带来的"一次绑定、单管线打全场"红利
        // - eLegacy：按 mesh 分批 vkCmdDrawIndirect（mesh_count 次 + per-mesh push mesh_id +
        //            per-mesh rebind 2 ds + 每 m_pipeline_switch_period 个 mesh 切一次 PSO 并
        //            强制 rebind 全 3 ds + 重发 push_const），模拟工业引擎按材质排序后切 PSO 的
        //            host overhead；与 NaiveCpu::eLegacy 形成"工业悲观 baseline"语义对齐
        void setEmulationMode(eEmulationMode mode) override;

        // 仅 eLegacy 用：配置每多少个 mesh 切一次 PSO。默认 1（每 mesh 必切）。
        void setPipelineSwitchPeriod(uint32_t period) noexcept { m_pipeline_switch_period = (period == 0u ? 1u : period); }

        // ABL-CULL 消融：true → cullOnCpu 走 identity 填充。
        void setDisableCull(bool disabled) override { m_disable_cull = disabled; }

    private:
        struct FrameResources
        {
            render::VulkanCommandBufferObject  graphics_cmd;
            render::VulkanCommandBufferObject  transfer_cmd;
            render::VulkanFramebufferObject    fbo;
        };

        // 在 CPU 端做视锥剔除，更新 BenchmarkScene 的 host shadow。
        // 返回剔除后的总可见实例数（用于辅证；CSV 不直接写入此值）。
        uint32_t cullOnCpu();

    private:
        render::VulkanContext * m_context_p = nullptr;
        BenchmarkScene        * m_scene_p   = nullptr;

        std::vector<FrameResources> m_frame_resources;
        uint32_t                    m_current_frame_index = 0;

        render::VulkanPipeline m_graphics_pipeline;
        // eLegacy 用：与 m_graphics_pipeline 同 program / 同 layout，仅 cullMode 反向（eFront）。
        // 对封闭 mesh trackball 视角下视觉无差，用于 mesh 之间 toggle 模拟 PSO state switch。
        render::VulkanPipeline m_graphics_pipeline_b;
        GpuTimestampQueryPool  m_timestamp_pool;
        std::vector<bool>      m_frame_has_history;

        eEmulationMode m_mode = eEmulationMode::eSingle;
        uint32_t       m_pipeline_switch_period = 1u;  // eLegacy: 每 N 个 mesh 切一次 PSO，默认 1（每 mesh 必切）
        bool           m_disable_cull = false;

        bool m_created = false;
    };

}  // namespace lcf::benchmark
