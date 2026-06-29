#include "vk_core/context/QueueContext.h"
#include "vk_core/command/CommandBufferProxy.h"
#include "vk_core/command/CommandBufferAllocateInfo.h"
#include <ranges>
#include <vector>

namespace stdr = std::ranges;
namespace stdv = std::views;

namespace lcf::vkc {

std::error_code QueueContext::create(vk::Device device, uint32_t family_index, uint32_t queue_index) noexcept
{
    m_family_index = family_index;
    m_queue = device.getQueue(family_index, queue_index);
    if (auto ec = m_cmd_allocator.create(device, m_family_index, this)) { return ec; }
    if (auto ec = m_timeline.create(device)) { return ec; }
    return {};
}

std::expected<CommandBufferBatch, std::error_code> QueueContext::allocate(const CommandBufferAllocateInfo & info) noexcept
{
    return m_cmd_allocator.allocate(info);
}

std::expected<vk::SemaphoreSubmitInfo, std::error_code> QueueContext::submit(CommandBufferBatch && batch) noexcept
{
    if (batch.getValidationData() != this) { return std::unexpected(make_error_code(errc::command_buffer_batch_queue_mismatch)); }
    vk::SemaphoreSubmitInfo timeline_signal = m_timeline.advanceTarget().generateSubmitInfo();
    auto cmd_submit_infos = batch.getCommandBuffers() |
        stdv::transform([](vk::CommandBuffer cmd) { return vk::CommandBufferSubmitInfo {cmd}; }) |
        stdr::to<std::vector>();
    std::span<const vk::SemaphoreSubmitInfo> wait_infos = batch.getWaitInfos();
    auto signal_infos = batch.getSignalInfos() | stdr::to<std::vector>();
    signal_infos.emplace_back(timeline_signal);

    vk::SubmitInfo2 submit_info;
    submit_info.setWaitSemaphoreInfos(wait_infos)
        .setCommandBufferInfos(cmd_submit_infos)
        .setSignalSemaphoreInfos(signal_infos);
    try {
        m_queue.submit2(submit_info);
    } catch (const vk::SystemError & e) {
        m_cmd_allocator.retire(std::move(batch), 0u);
        return std::unexpected(e.code());
    }
    uint64_t target_timestamp = m_timeline.getTargetTimestamp();
    m_lease_batch_queue.emplace(target_timestamp, batch.takeLeases());
    m_cmd_allocator.retire(std::move(batch), target_timestamp);
    return timeline_signal;
}


void QueueContext::collectGarbage() noexcept
{
    auto expected_timestamp = m_timeline.getCurrentTimestamp();
    if (not expected_timestamp) { return; }
    auto completed_timestamp = expected_timestamp.value();
    m_cmd_allocator.recycle(completed_timestamp);
    while (not m_lease_batch_queue.empty()) {
        auto & [timestamp, leases] = m_lease_batch_queue.front();
        if (timestamp > completed_timestamp) { break; }
        m_lease_batch_queue.pop();
    }
}

} // namespace lcf::vkc