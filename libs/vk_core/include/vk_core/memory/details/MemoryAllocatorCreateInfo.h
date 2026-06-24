#pragma once

#include <vulkan/vulkan.hpp>

namespace lcf::vkc::details {

class MemoryAllocatorCreateInfo
{
    using Self = MemoryAllocatorCreateInfo;
public:
    ~MemoryAllocatorCreateInfo() noexcept = default;
    MemoryAllocatorCreateInfo() noexcept = default;
    MemoryAllocatorCreateInfo(const Self &) = default;
    MemoryAllocatorCreateInfo(Self &&) noexcept = default;
    Self & operator =(const Self &) = default;
    Self & operator =(Self &&) noexcept = default;
public:
    Self & setBufferDeviceAddress(bool enabled) noexcept
    {
        m_buffer_device_address = enabled;
        return *this;
    }
    Self & setMemoryBudget(bool enabled) noexcept
    {
        m_memory_budget = enabled;
        return *this;
    }
    Self & setMemoryPriority(bool enabled) noexcept
    {
        m_memory_priority = enabled;
        return *this;
    }
    Self & setDeviceCoherentMemory(bool enabled) noexcept
    {
        m_device_coherent_memory = enabled;
        return *this;
    }
    // The allocator's own bookkeeping is not internally synchronised; the caller guarantees external synchronisation.
    Self & setExternallySynchronized(bool enabled) noexcept
    {
        m_externally_synchronized = enabled;
        return *this;
    }
    // 0 keeps the allocator's default block size.
    Self & setPreferredLargeHeapBlockSize(vk::DeviceSize size) noexcept
    {
        m_preferred_large_heap_block_size = size;
        return *this;
    }
    bool isBufferDeviceAddressEnabled() const noexcept { return m_buffer_device_address; }
    bool isMemoryBudgetEnabled() const noexcept { return m_memory_budget; }
    bool isMemoryPriorityEnabled() const noexcept { return m_memory_priority; }
    bool isDeviceCoherentMemoryEnabled() const noexcept { return m_device_coherent_memory; }
    bool isExternallySynchronized() const noexcept { return m_externally_synchronized; }
    vk::DeviceSize getPreferredLargeHeapBlockSize() const noexcept { return m_preferred_large_heap_block_size; }
private:
    bool m_buffer_device_address = false;
    bool m_memory_budget = false;
    bool m_memory_priority = false;
    bool m_device_coherent_memory = false;
    bool m_externally_synchronized = false;
    vk::DeviceSize m_preferred_large_heap_block_size = 0;
};

} // namespace lcf::vkc::details
