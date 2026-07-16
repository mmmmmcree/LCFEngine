#pragma once

#include <vulkan/vulkan.hpp>
#include <vector>
#include <deque>
#include <expected>
#include "vk_core/command/enums.h"

namespace lcf::vkc::details {

class PooledCommandBuffers
{
    using Self = PooledCommandBuffers;
    using CommandBufferList = std::vector<vk::CommandBuffer>;
public:
    ~PooledCommandBuffers() noexcept = default;
    PooledCommandBuffers() noexcept = default;
    PooledCommandBuffers(const Self &) = default;
    PooledCommandBuffers(Self &&) noexcept = default;
    Self & operator=(const Self &) = default;
    Self & operator=(Self &&) noexcept = default;
public:
    CommandBufferList acquire(uint32_t count) noexcept;
    void recycle(CommandBufferList && cmd_buffers) noexcept;
    void recycle(Self && cmd_buffers) noexcept;
    void clear() noexcept { m_cmd_free_list.clear(); }
private:
    CommandBufferList m_cmd_free_list;
};

class ResetableCommandPool
{
    using Self = ResetableCommandPool;
    using CommandBufferList = std::vector<vk::CommandBuffer>;
    struct PendingEntry
    {
        PendingEntry(uint64_t timestamp, CommandBufferList && cmd_batch) :
            m_timestamp(timestamp), m_cmd_buffers(std::move(cmd_batch)) {}
        uint64_t m_timestamp = 0;
        CommandBufferList m_cmd_buffers; 
    };
    using PendingEntryDeque = std::deque<PendingEntry>;
public:
    ~ResetableCommandPool() noexcept = default;
    ResetableCommandPool() noexcept = default;
    ResetableCommandPool(const Self &) = delete;
    ResetableCommandPool(Self &&) = default;
    Self & operator=(const Self &) = delete;
    Self & operator=(Self &&) = default;
public:
    std::error_code create(
        vk::Device device,
        const vk::CommandPoolCreateInfo & pool_info,
        vk::CommandBufferLevel level) noexcept;
    std::expected<CommandBufferList, std::error_code> allocate(uint32_t count) noexcept;
    void retire(uint64_t timestamp, CommandBufferList && cmd_batch) noexcept;
    void recycle(uint64_t completed_timestamp) noexcept;
private:
    vk::Device m_device;
    vk::CommandBufferLevel m_cmd_level;
    vk::UniqueCommandPool m_cmd_pool;
    PendingEntryDeque m_pending_entries;
    PooledCommandBuffers m_reusable_buffers;
};

class RotatingCommandPool
{
    using Self = RotatingCommandPool;
    using CommandBufferList = std::vector<vk::CommandBuffer>;
    struct PoolSlot
    {
        bool isValid() const noexcept { return m_cmd_pool.get(); }
        uint64_t m_timestamp;
        vk::UniqueCommandPool m_cmd_pool;
        PooledCommandBuffers m_reusable_buffers;
    };
    using PoolSlotDeque = std::deque<PoolSlot>;
public:
    ~RotatingCommandPool() noexcept = default;
    RotatingCommandPool() noexcept = default;
    RotatingCommandPool(const Self &) = delete;
    RotatingCommandPool(Self &&) = default;
    Self & operator=(const Self &) = delete;
    Self & operator=(Self &&) = default;
public:
    std::error_code create(
        vk::Device device,
        const vk::CommandPoolCreateInfo & pool_info,
        vk::CommandBufferLevel level) noexcept;
    std::expected<CommandBufferList, std::error_code> allocate(uint32_t count) noexcept;
    void retire(uint64_t timestamp, CommandBufferList && cmd_batch) noexcept;
    void recycle(uint64_t completed_timestamp) noexcept;
private:
    std::error_code activateSlot() noexcept;
private:
    vk::Device m_device;
    vk::CommandPoolCreateInfo m_pool_info;
    vk::CommandBufferLevel m_cmd_level;
    PoolSlot m_active_slot;
    uint64_t m_pending_timestamp = 0;
    PooledCommandBuffers m_pending_cmds;
    PoolSlot m_sealed_slot;
    PoolSlotDeque m_reusable_slots;
};

} // namespace lcf::vkc