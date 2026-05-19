#include "GpuTimestampQueryPool.h"

#include <stdexcept>

#include "Vulkan/VulkanCommandBufferObject.h"
#include "Vulkan/VulkanContext.h"
#include "log.h"

namespace lcf::benchmark {

    GpuTimestampQueryPool::~GpuTimestampQueryPool() noexcept
    {
        // vk::UniqueQueryPool 会自动销毁；m_context_p 仅观察指针。
    }

    void GpuTimestampQueryPool::create(render::VulkanContext * context, uint32_t frames_in_flight)
    {
        if (not context) {
            throw std::invalid_argument("GpuTimestampQueryPool::create: context is null");
        }
        m_context_p = context;
        m_frames_in_flight = frames_in_flight;
        m_compute_written.assign(frames_in_flight, false);

        const auto props = context->getPhysicalDevice().getProperties();
        m_timestamp_period_ns = static_cast<double>(props.limits.timestampPeriod);
        if (m_timestamp_period_ns <= 0.0) {
            // 极少数驱动会返回 0 — 退化为 1 ns/tick 防止除零；M2/M4 此时不可信。
            lcf_log_warn("GpuTimestampQueryPool: timestampPeriod=0 from driver, falling back to 1.0");
            m_timestamp_period_ns = 1.0;
        }

        const uint32_t slot_count = frames_in_flight * eSlotKindCount;
        vk::QueryPoolCreateInfo info;
        info.setQueryType(vk::QueryType::eTimestamp)
            .setQueryCount(slot_count);
        m_pool = context->getDevice().createQueryPoolUnique(info);

        // 注意：不在 host 端调 device.resetQueryPool —— 那需要 hostQueryReset feature
        // (VUID-vkResetQueryPool-None-02665)。改为依赖每帧 resetFrame() 内的
        // cmd.resetQueryPool 进行 device-side 重置；这一调用先于任何
        // writeTimestamp / getQueryPoolResults，等价于 host reset。
    }

    void GpuTimestampQueryPool::resetFrame(VulkanCmdBuffer & cmd, uint32_t frame_index)
    {
        cmd.resetQueryPool(m_pool.get(),
                           getSlotIndex(frame_index, eComputeBegin),
                           eSlotKindCount);
        m_compute_written[frame_index] = false;
    }

    void GpuTimestampQueryPool::writeComputeBegin(
        VulkanCmdBuffer & cmd, uint32_t frame_index, vk::PipelineStageFlagBits2 stage_mask)
    {
        cmd.writeTimestamp2(stage_mask, m_pool.get(), getSlotIndex(frame_index, eComputeBegin));
        m_compute_written[frame_index] = true;
    }
    void GpuTimestampQueryPool::writeComputeEnd(
        VulkanCmdBuffer & cmd, uint32_t frame_index, vk::PipelineStageFlagBits2 stage_mask)
    {
        cmd.writeTimestamp2(stage_mask, m_pool.get(), getSlotIndex(frame_index, eComputeEnd));
    }
    void GpuTimestampQueryPool::writeGraphicsBegin(
        VulkanCmdBuffer & cmd, uint32_t frame_index, vk::PipelineStageFlagBits2 stage_mask)
    {
        cmd.writeTimestamp2(stage_mask, m_pool.get(), getSlotIndex(frame_index, eGraphicsBegin));
    }
    void GpuTimestampQueryPool::writeGraphicsEnd(
        VulkanCmdBuffer & cmd, uint32_t frame_index, vk::PipelineStageFlagBits2 stage_mask)
    {
        cmd.writeTimestamp2(stage_mask, m_pool.get(), getSlotIndex(frame_index, eGraphicsEnd));
    }

    GpuTimestampQueryPool::FrameTimings GpuTimestampQueryPool::readBackFrame(uint32_t frame_index) const
    {
        FrameTimings ts;
        if (not m_pool.get()) { return ts; }

        // 关键：CpuIndirect / NaiveCpu 路径不写 ComputeBegin/ComputeEnd 这两个 slot；
        // 若连续 4 个 slot 用 eWait 一起读，驱动会无限等待 compute slot 的值，导致渲染线程永久阻塞。
        // 改为按段单独读，且按 m_compute_written[frame_index] 决定是否读 compute 段。
        // 同时使用 vk::QueryResultFlagBits::e64 + ePartial（或不 eWait）避免阻塞；
        // 若数据未就绪就视为缺失值。

        // ---- graphics 段：始终读（所有 renderer 都写）----
        std::array<uint64_t, 2> graphics_values {0, 0};
        const auto graphics_base = getSlotIndex(frame_index, eGraphicsBegin);
        auto gres = m_context_p->getDevice().getQueryPoolResults(
            m_pool.get(),
            graphics_base,
            2u,
            sizeof(graphics_values),
            graphics_values.data(),
            sizeof(uint64_t),
            vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWait);
        if (gres == vk::Result::eSuccess) {
            const auto graphics_diff = graphics_values[1] > graphics_values[0]
                                           ? (graphics_values[1] - graphics_values[0])
                                           : 0ull;
            ts.m_gpu_frame_ms    = static_cast<double>(graphics_diff) * m_timestamp_period_ns / 1.0e6;
            ts.m_graphics_valid  = graphics_diff > 0;
        }

        // ---- compute 段：仅当本帧确实 write 过才读，否则跳过 ----
        if (m_compute_written[frame_index]) {
            std::array<uint64_t, 2> compute_values {0, 0};
            const auto compute_base = getSlotIndex(frame_index, eComputeBegin);
            auto cres = m_context_p->getDevice().getQueryPoolResults(
                m_pool.get(),
                compute_base,
                2u,
                sizeof(compute_values),
                compute_values.data(),
                sizeof(uint64_t),
                vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWait);
            if (cres == vk::Result::eSuccess) {
                const auto compute_diff = compute_values[1] > compute_values[0]
                                              ? (compute_values[1] - compute_values[0])
                                              : 0ull;
                ts.m_gpu_cull_ms   = static_cast<double>(compute_diff) * m_timestamp_period_ns / 1.0e6;
                ts.m_compute_valid = compute_diff > 0;
            }
        }
        return ts;
    }

}  // namespace lcf::benchmark
