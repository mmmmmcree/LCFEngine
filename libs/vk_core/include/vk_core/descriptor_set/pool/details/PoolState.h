#pragma once

#include <vulkan/vulkan.hpp>
#include <cstdint>
#include <flat_map>

namespace lcf::vkc::dsp::details {

using PoolCapacityMap = std::flat_map<vk::DescriptorType, uint32_t>;

class PoolState
{
    using Self = PoolState;
public:
    ~PoolState() noexcept = default;
    PoolState() noexcept = default;
    PoolState(const Self &) noexcept = default;
    PoolState(Self &&) noexcept = default;
    Self & operator=(const Self &) noexcept = default;
    Self & operator=(Self &&) noexcept = default;
public:
    void create(uint32_t max_set_count, PoolCapacityMap capacity_map) noexcept;
    bool fits(uint32_t required_set_count, const PoolCapacityMap & required_capacity) const noexcept;
    void consume(uint32_t required_set_count, const PoolCapacityMap & required_capacity) noexcept;
    uint64_t getPriority() const noexcept;
    bool isDestroyable() const noexcept { return m_allocated_set_count == m_deallocated_set_count; }
    void markDeallocate(uint32_t deallocated_set_count = 1u) noexcept { m_deallocated_set_count += deallocated_set_count; }
private:
    uint32_t m_remaining_set_count = 0u;
    uint32_t m_allocated_set_count = 0u;
    uint32_t m_deallocated_set_count = 0u;
    PoolCapacityMap m_remaining_capacity;
};

} // namespace lcf::vkc::dsp::details
