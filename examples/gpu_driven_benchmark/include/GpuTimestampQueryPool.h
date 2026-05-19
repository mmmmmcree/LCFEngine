#pragma once

// GpuTimestampQueryPool：包装 vk::QueryPool 给 benchmark 用的"每帧 GPU 时间戳查询"基础设施。
//
// 每帧分配 4 个 slot（按 frame-in-flight 翻转）：
//   slot[0] = compute_begin   — 计算阶段开始（仅 GpuDriven 路径写入；其它路径不写）
//   slot[1] = compute_end     — 计算阶段结束（同上）
//   slot[2] = graphics_begin  — 图形阶段开始（M2 起点；三路径都写）
//   slot[3] = graphics_end    — 图形阶段结束（M2 终点）
//
// 总 slot 数 = frames_in_flight * 4，pool 在构造时一次性分配。
//
// readBackPreviousFrame 用 eWait | e64 同步回读，保证拿到的差值是 GPU 已完成的；
// 这是 benchmark 场景的合理选择（一致性 > 端到端最低延迟）。

#include <cstdint>
#include <span>
#include <vector>

#include <vulkan/vulkan.hpp>

namespace lcf::render { class VulkanContext; class VulkanCommandBufferObject; }

namespace lcf::benchmark {

    class GpuTimestampQueryPool
    {
        using VulkanCmdBuffer = render::VulkanCommandBufferObject;
    public:
        struct FrameTimings
        {
            double m_gpu_frame_ms = 0.0;  // graphics_end - graphics_begin
            double m_gpu_cull_ms  = 0.0;  // compute_end - compute_begin（无写入则为 0）
            bool   m_compute_valid  = false;
            bool   m_graphics_valid = false;
        };

        GpuTimestampQueryPool() = default;
        ~GpuTimestampQueryPool() noexcept;
        GpuTimestampQueryPool(const GpuTimestampQueryPool &) = delete;
        GpuTimestampQueryPool & operator=(const GpuTimestampQueryPool &) = delete;

        void create(render::VulkanContext * context, uint32_t frames_in_flight);

        // 在每个 cmd 真正录制之前调用：把该帧 4 个 slot reset 为未写入状态。
        // reset 必须在命令缓冲 begin() 之后、第一次 writeTimestamp 之前。
        void resetFrame(VulkanCmdBuffer & cmd, uint32_t frame_index);

        // 写入对应阶段的开始/结束时间戳。stage_mask 应与所写入的 cmd buffer 阶段匹配。
        // 仅 GpuDriven 路径会调用 writeComputeBegin/End；其它路径直接跳过。
        void writeComputeBegin (VulkanCmdBuffer & cmd, uint32_t frame_index, vk::PipelineStageFlagBits2 stage_mask);
        void writeComputeEnd   (VulkanCmdBuffer & cmd, uint32_t frame_index, vk::PipelineStageFlagBits2 stage_mask);
        void writeGraphicsBegin(VulkanCmdBuffer & cmd, uint32_t frame_index, vk::PipelineStageFlagBits2 stage_mask);
        void writeGraphicsEnd  (VulkanCmdBuffer & cmd, uint32_t frame_index, vk::PipelineStageFlagBits2 stage_mask);

        // 同步回读上一轮在 frame_index 这一格上写入的时间戳（GPU 已完成）。
        // 若该帧未写入 compute 段，返回的 m_compute_valid = false。
        FrameTimings readBackFrame(uint32_t frame_index) const;

    private:
        enum SlotKind : uint32_t {
            eComputeBegin  = 0,
            eComputeEnd    = 1,
            eGraphicsBegin = 2,
            eGraphicsEnd   = 3,
            eSlotKindCount = 4,
        };

        uint32_t getSlotIndex(uint32_t frame_index, SlotKind kind) const noexcept
        {
            return frame_index * eSlotKindCount + static_cast<uint32_t>(kind);
        }

    private:
        render::VulkanContext * m_context_p = nullptr;
        vk::UniqueQueryPool      m_pool;
        uint32_t                 m_frames_in_flight = 0;
        double                   m_timestamp_period_ns = 1.0;  // 由 physical device 提供（单位 ns）
        // 标记每帧 compute 段是否被写入，用于回读时判断 m_compute_valid。
        std::vector<bool>        m_compute_written;
    };

}  // namespace lcf::benchmark
