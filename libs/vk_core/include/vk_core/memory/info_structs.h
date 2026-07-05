#pragma once

#include <vulkan/vulkan.hpp>
#include "vk_core/memory/enums.h"

namespace lcf::vkc {

class MemoryAllocationInfo
{
    using Self = MemoryAllocationInfo;
public:
    ~MemoryAllocationInfo() noexcept = default;
    MemoryAllocationInfo() noexcept = default;
    MemoryAllocationInfo(const Self &) = default;
    MemoryAllocationInfo(Self &&) noexcept = default;
    Self & operator =(const Self &) = default;
    Self & operator =(Self &&) noexcept = default;
public:
    Self & setAccess(MemoryAccess access) noexcept
    {
        m_access = access;
        return *this;
    }
    Self & setStrategy(AllocationStrategy strategy) noexcept
    {
        m_strategy = strategy;
        return *this;
    }
    // Hint in [0, 1] used when this allocation gets its own memory block; higher means more likely to stay resident.
    Self & setPriority(float priority) noexcept
    {
        m_priority = priority;
        return *this;
    }
    Self & setDedicated(bool dedicated) noexcept
    {
        m_dedicated = dedicated;
        return *this;
    }
    Self & setWithinBudget(bool within_budget) noexcept
    {
        m_within_budget = within_budget;
        return *this;
    }
    MemoryAccess getAccess() const noexcept { return m_access; }
    AllocationStrategy getStrategy() const noexcept { return m_strategy; }
    float getPriority() const noexcept { return m_priority; }
    bool isDedicated() const noexcept { return m_dedicated; }
    bool isWithinBudget() const noexcept { return m_within_budget; }
private:
    MemoryAccess m_access = MemoryAccess::eDeviceLocal;
    AllocationStrategy m_strategy = AllocationStrategy::eDefault;
    float m_priority = 0.5f;
    bool m_dedicated = false;
    bool m_within_budget = false;
};

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

} // namespace lcf::vkc
