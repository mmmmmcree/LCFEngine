#include "vk_core/command/details/CommandBufferAllocator.h"
#include "vk_core/command/CommandBufferAllocateInfo.h"
#include "vk_core/command/CommandBufferProxy.h"
#include <algorithm>

namespace stdr = std::ranges;

namespace lcf::vkc::details {

std::error_code CommandBufferAllocator::create(vk::Device device, uint32_t family_index, ValidationData validation_data) noexcept
{
    m_device = device;
    m_family_index = family_index;
    m_validation_data = validation_data;
    return {};
}

std::expected<CommandBufferProxy, std::error_code> CommandBufferAllocator::allocate(const CommandBufferAllocateInfo & info) noexcept
{
    auto expected_sub_pool = this->acquireSubPool(info.getUsageFlags());
    if (not expected_sub_pool) { return std::unexpected(expected_sub_pool.error()); }
    SubPool & sub_pool = *expected_sub_pool.value();
    vk::CommandBuffer buffer;
    if (not sub_pool.m_free_list.empty()) {
        buffer = sub_pool.m_free_list.back();
        sub_pool.m_free_list.pop_back();
    } else {
        try {
            buffer = m_device.allocateCommandBuffers({sub_pool.m_pool.get(), info.getLevel(), 1u}).front();
        } catch (const vk::SystemError & e) {
            return std::unexpected(e.code());
        }
    }
    ++sub_pool.m_orphan_count;
    return CommandBufferProxy {buffer, info.getUsageFlags(), m_validation_data};
}

void CommandBufferAllocator::retire(CommandBufferProxy && proxy, uint64_t timestamp) noexcept
{
    if (proxy.m_validation_data != m_validation_data) { return; }
    auto it = m_sub_pools.find(VkCommandPoolCreateFlags(proxy.m_usage_flags));
    if (it == m_sub_pools.end()) { return; }
    SubPool & sub_pool = it->second;
    sub_pool.m_pending_entries.emplace_back(PendingEntry {timestamp, proxy, std::move(proxy.m_leases)});
    --sub_pool.m_orphan_count;
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
            if (entry.m_timestamp > completed_timestamp) { return; }
            entry.m_buffer.reset({});
            sub_pool.m_free_list.emplace_back(entry.m_buffer);
            pending_entries.pop_front();   //- drops leases
        }
        return;
    }
    if (sub_pool.m_orphan_count > 0u or sub_pool.m_pending_entries.empty()) { return; }
    bool all_completed = stdr::all_of(sub_pool.m_pending_entries, std::bind_back(std::less_equal<>{}, completed_timestamp), &PendingEntry::m_timestamp);
    if (not all_completed) { return; }
    m_device.resetCommandPool(sub_pool.m_pool.get(), {});
    for (PendingEntry & entry : sub_pool.m_pending_entries) {
        sub_pool.m_free_list.emplace_back(entry.m_buffer);
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
