#pragma once

#include <vulkan/vulkan.hpp>
#include <deque>
#include <expected>
#include <unordered_map>
#include <system_error>
#include <vector>
#include "resource_utils.h"

namespace lcf::vkc {

class CommandBufferAllocateInfo;
class CommandBufferProxy;

} // namespace lcf::vkc

namespace lcf::vkc::details {

class CommandBufferAllocator
{
    using Self = CommandBufferAllocator;
    struct PendingEntry
    {
        uint64_t m_timestamp = 0u;
        vk::CommandBuffer m_buffer = nullptr;
        std::vector<ResourceLease> m_leases;
    };
    struct SubPool
    {
        vk::UniqueCommandPool m_pool;
        std::vector<vk::CommandBuffer> m_free_list;
        std::deque<PendingEntry> m_pending_entries;
        uint32_t m_orphan_count = 0u;            //- allocated, not yet retired (whole-pool reset hazard)
    };
    using SubPoolMap = std::unordered_map<VkCommandPoolCreateFlags, SubPool>;
    using ValidationData = const void *;
public:
    ~CommandBufferAllocator() noexcept = default;
    CommandBufferAllocator() = default;
    CommandBufferAllocator(const Self &) = delete;
    Self & operator=(const Self &) = delete;
    CommandBufferAllocator(Self &&) noexcept = default;
    Self & operator=(Self &&) noexcept = default;
public:
    std::error_code create(vk::Device device, uint32_t family_index, ValidationData validation_data) noexcept;
    std::expected<CommandBufferProxy, std::error_code> allocate(const CommandBufferAllocateInfo & info) noexcept;
    void retire(CommandBufferProxy && proxy, uint64_t timestamp) noexcept;
    void recycle(uint64_t completed) noexcept;
private:
    std::expected<SubPool *, std::error_code> acquireSubPool(vk::CommandPoolCreateFlags flags) noexcept;
    void recycleSubPool(vk::CommandPoolCreateFlags flags, SubPool & sub_pool, uint64_t completed_timestamp) noexcept;
private:
    vk::Device m_device;
    uint32_t m_family_index = 0u;
    ValidationData m_validation_data = nullptr;
    SubPoolMap m_sub_pools;
};

} // namespace lcf::vkc::details
