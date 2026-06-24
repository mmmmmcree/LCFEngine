#pragma once

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

} // namespace lcf::vkc
