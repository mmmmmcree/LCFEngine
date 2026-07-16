#include "vk_core/command/details/command_pools.h"
#include <ranges>

namespace stdr = std::ranges;
namespace stdv = std::views;

namespace lcf::vkc::details {

auto PooledCommandBuffers::acquire(uint32_t count) noexcept -> CommandBufferList
{
    uint32_t reused_count = std::min(count, static_cast<uint32_t>(m_cmd_free_list.size()));
    auto result = m_cmd_free_list | stdv::reverse | stdv::take(reused_count) | stdr::to<std::vector>();
    m_cmd_free_list.resize(static_cast<uint32_t>(m_cmd_free_list.size()) - reused_count);
    return result;
}

void PooledCommandBuffers::recycle(CommandBufferList && cmd_buffers) noexcept
{
    m_cmd_free_list.append_range(std::move(cmd_buffers));
}

void PooledCommandBuffers::recycle(Self && cmd_buffers) noexcept
{
    m_cmd_free_list.append_range(std::move(cmd_buffers.m_cmd_free_list));
}


std::error_code ResetableCommandPool::create(
    vk::Device device,
    const vk::CommandPoolCreateInfo &pool_info,
    vk::CommandBufferLevel level) noexcept
{
    m_device = device;
    m_cmd_level = level;
    try {
        m_cmd_pool = m_device.createCommandPoolUnique(pool_info);
    } catch (const vk::SystemError & e) {
        return e.code();
    } 
    return {};
}

auto ResetableCommandPool::allocate(uint32_t count) noexcept -> std::expected<CommandBufferList, std::error_code>
{
    CommandBufferList cmd_buffers =  m_reusable_buffers.acquire(count);
    if (static_cast<uint32_t>(cmd_buffers.size()) == count) { return cmd_buffers; }
    uint32_t remaining_count = count - static_cast<uint32_t>(cmd_buffers.size());
    try {
        auto allocated = m_device.allocateCommandBuffers({m_cmd_pool.get(), m_cmd_level, remaining_count});
        cmd_buffers.append_range(std::move(allocated));
    } catch (const vk::SystemError & e) {
        m_reusable_buffers.recycle(std::move(cmd_buffers));
        return std::unexpected(e.code());
    }
    return cmd_buffers;
}

void ResetableCommandPool::retire(uint64_t timestamp, CommandBufferList &&cmd_batch) noexcept
{
    m_pending_entries.emplace_back(timestamp, std::move(cmd_batch));
}

void ResetableCommandPool::recycle(uint64_t completed_timestamp) noexcept
{
    while (not m_pending_entries.empty()) {
        PendingEntry & entry = m_pending_entries.front();
        if (entry.m_timestamp > completed_timestamp) { break; }
        for (vk::CommandBuffer buffer : entry.m_cmd_buffers) { buffer.reset({}); }
        m_reusable_buffers.recycle(std::move(entry.m_cmd_buffers));
        m_pending_entries.pop_front();
    }
}


std::error_code RotatingCommandPool::create(
    vk::Device device,
    const vk::CommandPoolCreateInfo &pool_info,
    vk::CommandBufferLevel level) noexcept
{
    m_device = device;
    m_pool_info = pool_info;
    m_cmd_level = level;
    return {};
}

auto RotatingCommandPool::allocate(uint32_t count) noexcept -> std::expected<CommandBufferList, std::error_code>
{
    if (not m_active_slot.isValid()) {
        if (auto ec = this->activateSlot()) { return std::unexpected(ec); }
    }
    CommandBufferList cmd_buffers =  m_active_slot.m_reusable_buffers.acquire(count);
    if (static_cast<uint32_t>(cmd_buffers.size()) == count) { return cmd_buffers; }
    uint32_t remaining_count = count - static_cast<uint32_t>(cmd_buffers.size());
    try {
        auto allocated = m_device.allocateCommandBuffers({m_active_slot.m_cmd_pool.get(), m_cmd_level, remaining_count});
        cmd_buffers.append_range(std::move(allocated));
    } catch (const vk::SystemError & e) {
        m_active_slot.m_reusable_buffers.recycle(std::move(cmd_buffers));
        return std::unexpected(e.code());
    }
    return cmd_buffers;
}

void RotatingCommandPool::retire(uint64_t timestamp, CommandBufferList &&cmd_batch) noexcept
{
    m_pending_timestamp = timestamp;
    m_pending_cmds.recycle(std::move(cmd_batch));
}

void RotatingCommandPool::recycle(uint64_t completed_timestamp) noexcept
{
    if (not m_sealed_slot.isValid()) {
        m_sealed_slot = std::move(m_active_slot);
        m_sealed_slot.m_timestamp = m_pending_timestamp;
        m_sealed_slot.m_reusable_buffers.recycle(std::move(m_pending_cmds));
    }
    if (m_sealed_slot.isValid() and completed_timestamp >= m_sealed_slot.m_timestamp) {
        m_device.resetCommandPool(m_sealed_slot.m_cmd_pool.get());
        m_reusable_slots.emplace_back(std::move(m_sealed_slot));
    }
}

std::error_code RotatingCommandPool::activateSlot() noexcept
{
    if (not m_reusable_slots.empty()) {
        m_active_slot = std::move(m_reusable_slots.front());
        m_reusable_slots.pop_front();
        return {};
    }
    try {
        m_active_slot.m_cmd_pool = m_device.createCommandPoolUnique(m_pool_info);
    } catch (const vk::SystemError & e) {
        return e.code();
    }
    return {};
}

} // namespace lcf::vkc::details

