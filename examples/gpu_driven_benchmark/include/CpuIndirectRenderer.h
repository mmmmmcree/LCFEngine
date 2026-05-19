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
        GpuTimestampQueryPool  m_timestamp_pool;
        std::vector<bool>      m_frame_has_history;

        bool m_created = false;
    };

}  // namespace lcf::benchmark
