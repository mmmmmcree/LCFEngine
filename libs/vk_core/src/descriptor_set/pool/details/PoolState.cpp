#include "vk_core/descriptor_set/pool/details/PoolState.h"

#include <algorithm>
#include <ranges>
#include <utility>

namespace stdr = std::ranges;
namespace stdv = std::views;

namespace lcf::vkc::dsp::details {

void PoolState::create(uint32_t max_set_count, PoolCapacityMap capacity_map) noexcept
{
    m_remaining_set_count = max_set_count;
    m_allocated_set_count = 0u;
    m_deallocated_set_count = 0u;
    m_remaining_capacity = std::move(capacity_map);
}

bool PoolState::fits(uint32_t required_set_count, const PoolCapacityMap & required_capacity) const noexcept
{
    if (m_remaining_set_count < required_set_count) { return false; }
    return stdr::all_of(required_capacity, [this, required_set_count](const auto & required) noexcept {
        const auto &[descriptor_type, descriptor_count] = required;
        const auto it = m_remaining_capacity.find(descriptor_type);
        return it != m_remaining_capacity.end() and it->second >= descriptor_count * required_set_count;
    });
}

void PoolState::consume(uint32_t required_set_count, const PoolCapacityMap & required_capacity) noexcept
{
    m_remaining_set_count -= required_set_count;
    m_allocated_set_count += required_set_count;
    for (const auto &[descriptor_type, descriptor_count] : required_capacity) {
        m_remaining_capacity.find(descriptor_type)->second -= descriptor_count * required_set_count;
    }
}

uint64_t PoolState::getPriority() const noexcept
{
    uint32_t hint_value = m_remaining_set_count;
    if (not m_remaining_capacity.empty()) {
        hint_value = stdr::min(hint_value, stdr::min(m_remaining_capacity | stdv::values));
    }
    return (static_cast<uint64_t>(not this->isDestroyable()) << 32u) | hint_value;
}

} // namespace lcf::vkc::dsp::details
