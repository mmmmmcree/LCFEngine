#pragma once

// GpuDrivenRenderer：GPU 生成绘制路径。
//
// 与 RenderEngine/src/Vulkan/VulkanRenderer.cpp 的精简对位关系：
//   - pipeline：仅保留 compute (cull.comp) + graphics (vertex_buffer_test.*) 两条主流程
//     管线；剥离 skybox / sphere_to_cube 两个演示用流程。
//   - DGC：单一 sequence（meshes only），k_sequence_count = 1，简化 IndirectExecutionSet 与
//     IndirectCommandsLayout 配置；不再含 skybox 第二个 token slot。
//   - frame resources：3 命令缓冲 + 1 framebuffer，与 VulkanRenderer 完全一致。
//   - 共享数据来源：BenchmarkScene（PerView UBO / 5 路 SSBO / bindless DS / mesh / material）。
//   - 指标：每帧 chrono 包夹 record+submit 段（M1）；timestamp pool 写 compute_*/graphics_*
//     两对（M2/M4）；M3 = 1（一次 executeGeneratedCommandsEXT）。

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
#include "Vulkan/memory/VulkanBufferObject.h"

namespace lcf::benchmark {

    class GpuDrivenRenderer final : public IBenchmarkRenderer
    {
    public:
        GpuDrivenRenderer() = default;
        ~GpuDrivenRenderer() override;
        GpuDrivenRenderer(const GpuDrivenRenderer &) = delete;
        GpuDrivenRenderer & operator=(const GpuDrivenRenderer &) = delete;

        void create(render::VulkanContext * context,
                    BenchmarkScene * scene,
                    std::pair<uint32_t, uint32_t> max_extent,
                    ecs::Registry & registry) override;

        FrameMetrics render(const ecs::Entity & camera,
                            const ecs::Entity & render_target) override;

        ePath getPath() const noexcept override { return ePath::eGpuDriven; }

    private:
        // 与 VulkanRenderer.cpp::DrawSequence 同形（pipeline_index + draw_call）；
        // 但本路径只用 1 条 sequence（mesh only），pipeline_index 始终 = 0。
        struct DrawSequence
        {
            uint32_t                                          pipeline_index = 0;
            vk::DrawIndirectCountIndirectCommandEXT           draw_call;
        };

        struct FrameResources
        {
            render::VulkanCommandBufferObject  graphics_cmd;
            render::VulkanCommandBufferObject  transfer_cmd;
            render::VulkanCommandBufferObject  compute_cmd;
            render::VulkanFramebufferObject    fbo;
        };

    private:
        void initPipelines();
        void initDgc();
        void initFrameResources(std::pair<uint32_t, uint32_t> max_extent);

    private:
        render::VulkanContext * m_context_p = nullptr;
        BenchmarkScene        * m_scene_p   = nullptr;

        std::vector<FrameResources>  m_frame_resources;
        uint32_t                     m_current_frame_index = 0;

        render::VulkanPipeline       m_compute_pipeline;
        render::VulkanPipeline       m_graphics_pipeline;

        // DGC 资源（与 VulkanRenderer.cpp::create 行 367-429 严格对位）
        vk::UniqueIndirectExecutionSetEXT  m_indirect_execution_set;
        vk::UniqueIndirectCommandsLayoutEXT m_indirect_commands_layout;
        render::VulkanBufferObject        m_preprocess_buffer;
        render::VulkanBufferObject        m_sequence_buffer;

        GpuTimestampQueryPool m_timestamp_pool;

        bool m_created = false;

        // 跑分回读相关：第一帧没有上一轮可读，FrameMetrics.m2/m4 = 0。
        std::vector<bool> m_frame_has_history;
    };

}  // namespace lcf::benchmark
