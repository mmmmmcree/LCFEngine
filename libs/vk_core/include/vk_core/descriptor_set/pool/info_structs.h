#pragma once

#include <vulkan/vulkan.hpp>
#include <flat_map>
#include <array>
#include "enums/enum_count.h"
#include "concepts/range_concept.h"

namespace lcf::vkc::dsp {

class DescriptorSetAllocatorInfo
{
    using Self = DescriptorSetAllocatorInfo;
    using PoolSizes = std::flat_map<vk::DescriptorType, uint32_t>;
public:
    DescriptorSetAllocatorInfo() noexcept = default;
public:
    constexpr Self & setPoolSize(vk::DescriptorPoolSize pool_size) noexcept { m_pool_sizes.emplace(pool_size.type, pool_size.descriptorCount); return *this; }
    constexpr Self & setPoolSizes(range_of_c<vk::DescriptorPoolSize> auto && pool_sizes) noexcept
    {
        for (auto && pool_size : pool_sizes) { this->setPoolSize(pool_size); }
        return *this;
    };
    constexpr Self & setMaxSetsPerPool(uint32_t max_sets_per_pool) noexcept { m_max_sets_per_pool = max_sets_per_pool; return *this; }
    constexpr Self & setGrowthFactor(float growth_factor) noexcept { m_growth_factor = growth_factor; return *this; }
    const PoolSizes & getPoolSizes() const noexcept { return m_pool_sizes; }
    const uint32_t & getMaxSetsPerPool() const noexcept { return m_max_sets_per_pool; }
    const float & getGrowthFactor() const noexcept { return m_growth_factor; }
private:
    PoolSizes m_pool_sizes;
    uint32_t m_max_sets_per_pool = 8u;
    float m_growth_factor = 2.0f;
};

} // namespace lcf::vkc::dsp