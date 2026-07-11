#include "vk_core/command/details/CommandBufferAllocator.h"
#include "vk_core/command/CommandBufferAllocateInfo.h"
#include "vk_core/command/CommandBufferProxy.h"
#include <ranges>
#include <algorithm>

namespace stdr = std::ranges;
namespace stdv = std::views;

namespace lcf::vkc::details {

std::error_code CommandBufferAllocator::create(vk::Device device, uint32_t family_index, ValidationData validation_data) noexcept
{
    m_device = device;
    m_family_index = family_index;
    m_validation_data = validation_data;
    return {};
}

std::expected<CommandBufferBatch, std::error_code> CommandBufferAllocator::allocate(const CommandBufferAllocateInfo & info) noexcept
{
    auto expected_sub_pool = this->acquireSubPool(info.getUsageFlags());
    if (not expected_sub_pool) { return std::unexpected(expected_sub_pool.error()); }
    SubPool & sub_pool = *expected_sub_pool.value();
    uint32_t count = info.getCount();
    auto & free_list = sub_pool.m_free_list;
    uint32_t reused_count = std::min(count, static_cast<uint32_t>(free_list.size()));
    auto cmd_buffers = free_list | stdv::reverse | stdv::take(reused_count) | stdr::to<std::vector>();
    free_list.resize(free_list.size() - reused_count);
    uint32_t remaining_count = count - reused_count;
    if (remaining_count == 0u) {
        sub_pool.m_orphan_count += reused_count;
        return CommandBufferBatch {std::move(cmd_buffers), info.getUsageFlags(), m_validation_data};
    }
    try {
        auto allocated = m_device.allocateCommandBuffers({sub_pool.m_pool.get(), info.getLevel(), remaining_count});
        cmd_buffers.append_range(allocated);
    } catch (const vk::SystemError & e) {
        free_list.append_range(std::move(cmd_buffers));
        return std::unexpected(e.code());
    }
    sub_pool.m_orphan_count += count;
    return CommandBufferBatch {std::move(cmd_buffers), info.getUsageFlags(), m_validation_data};
}

void CommandBufferAllocator::retire(CommandBufferBatch && batch, uint64_t timestamp) noexcept
{
    if (batch.m_validation_data != m_validation_data) { return; }
    auto it = m_sub_pools.find(VkCommandPoolCreateFlags(batch.m_usage_flags));
    if (it == m_sub_pools.end()) { return; }
    SubPool & sub_pool = it->second;
    sub_pool.m_orphan_count -= static_cast<uint32_t>(batch.m_cmd_buffers.size());
    sub_pool.m_pending_entries.emplace_back(PendingEntry {timestamp, std::move(batch.m_cmd_buffers)});
}

void CommandBufferAllocator::recycle(uint64_t completed_timestamp) noexcept
{
    for (auto & [flags, sub_pool] : m_sub_pools) { this->recycleSubPool(vk::CommandPoolCreateFlags(flags), sub_pool, completed_timestamp); }
}

void CommandBufferAllocator::recycleSubPool(vk::CommandPoolCreateFlags flags, SubPool & sub_pool, uint64_t completed_timestamp) noexcept
{
    if (flags & vk::CommandPoolCreateFlagBits::eResetCommandBuffer) {
        auto & pending_entries = sub_pool.m_pending_entries;
        while (not pending_entries.empty()) {
            PendingEntry & entry = pending_entries.front();
            if (entry.m_timestamp > completed_timestamp) { break; }
            for (vk::CommandBuffer buffer : entry.m_cmd_buffers) {
                buffer.reset({});
                sub_pool.m_free_list.emplace_back(buffer);
            }
            pending_entries.pop_front();
        }
        return;
    }
    if (sub_pool.m_orphan_count > 0u or sub_pool.m_pending_entries.empty()) { return; }
    bool all_completed = stdr::all_of(sub_pool.m_pending_entries, std::bind_back(std::less_equal<>{}, completed_timestamp), &PendingEntry::m_timestamp);
    if (not all_completed) { return; }
    m_device.resetCommandPool(sub_pool.m_pool.get(), {});
    for (PendingEntry & entry : sub_pool.m_pending_entries) {
        sub_pool.m_free_list.append_range(entry.m_cmd_buffers);
    }
    sub_pool.m_pending_entries.clear();
}

auto CommandBufferAllocator::acquireSubPool(vk::CommandPoolCreateFlags flags) noexcept -> std::expected<SubPool *, std::error_code>
{
    auto it = m_sub_pools.find(VkCommandPoolCreateFlags(flags));
    if (it != m_sub_pools.end()) { return &it->second; }
    SubPool sub_pool;
    try {
        sub_pool.m_pool = m_device.createCommandPoolUnique({flags, m_family_index});
    } catch (const vk::SystemError & e) {
        return std::unexpected(e.code());
    }
    auto [inserted, ok] = m_sub_pools.emplace(flags, std::move(sub_pool));
    return &inserted->second;
}

} // namespace lcf::vkc::details
