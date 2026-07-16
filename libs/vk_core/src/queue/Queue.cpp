#include "vk_core/queue/Queue.h"
#include "vk_core/queue/entry.h"
#include "vk_core/sync/entry.h"
#include "vk_core/manifest/InstanceExtensionManifest.h"
#include "vk_core/manifest/DeviceExtensionManifest.h"
#include "vk_core/command/CommandBufferProxy.h"
#include "vk_core/command/info_structs.h"
#include <ranges>
#include <vector>

namespace stdr = std::ranges;
namespace stdv = std::views;

namespace lcf::vkc::entry {

void register_queue(InstanceExtensionManifest & inst_manifest, DeviceExtensionManifest & device_manifest) noexcept
{
    register_timeline_semaphore(device_manifest);
}

} // namespace lcf::vkc::entry

namespace lcf::vkc {

std::error_code Queue::create(const LogicalQueue & logical_queue) noexcept
{
    m_logical_queue = logical_queue;
    auto device = m_logical_queue.getDevice();
    auto queue_family_index = m_logical_queue.getFamilyIndex();
    if (auto ec = m_cmd_allocator.create(device, queue_family_index, this)) { return ec; }
    if (auto ec = m_timeline.create(device)) { return ec; }
    return {};
}

std::expected<CommandBufferBatch, std::error_code> Queue::allocateCommandBufferBatch(const CommandBufferAllocateInfo & info) noexcept
{
    return m_cmd_allocator.allocate(info);
}

std::expected<SubmissionToken, std::error_code> Queue::submit(CommandBufferBatch && batch) noexcept
{
    if (batch.getValidationData() != this) { return std::unexpected(make_error_code(errc::command_buffer_batch_queue_mismatch)); }
    SubmissionToken timeline_signal = m_timeline.advanceTarget().generateSubmitInfo();
    auto cmd_submit_infos = batch.getCommandBuffers() |
        stdv::transform([](vk::CommandBuffer cmd) { return vk::CommandBufferSubmitInfo {cmd}; }) |
        stdr::to<std::vector>();
    std::span<const SubmissionToken> wait_infos = batch.getWaitInfos();
    auto signal_infos = batch.getSignalInfos() | stdr::to<std::vector>();
    signal_infos.emplace_back(timeline_signal);

    vk::SubmitInfo2 submit_info;
    submit_info.setWaitSemaphoreInfos(wait_infos)
        .setCommandBufferInfos(cmd_submit_infos)
        .setSignalSemaphoreInfos(signal_infos);
    uint64_t target_timestamp = m_timeline.getTargetTimestamp();
    try {
        QueueAccess queue_access {m_logical_queue};
        queue_access->submit2(submit_info);
    } catch (const vk::SystemError & e) {
        m_cmd_allocator.retire(target_timestamp, std::move(batch));
        return std::unexpected(e.code());
    }
    m_lease_batch_queue.emplace(target_timestamp, batch.takeLeases());
    m_cmd_allocator.retire(target_timestamp, std::move(batch));
    return timeline_signal;
}


void Queue::collectGarbage() noexcept
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