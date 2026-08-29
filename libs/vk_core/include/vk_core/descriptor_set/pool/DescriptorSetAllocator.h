#pragma once

#include <vulkan/vulkan.hpp>
#include "info_structs.h"
#include "details/PoolState.h"
#include "resource_utils.h"
#include <expected>
#include <flat_map>
#include <flat_set>
#include <span>
#include <vector>
#include <deque>

namespace lcf::vkc {

class DescriptorSetLayoutInfo;

}

namespace lcf::vkc::dsp {

class DescriptorSetLayout;
class DescriptorSetProxy;

struct DescriptorSetAllocation
{
    vk::DescriptorPool m_pool;
    VkDescriptorPoolCreateFlags m_pool_key;
    std::vector<vk::DescriptorSet> m_descriptor_sets;
};

class DescriptorSetAllocator
{
    friend class DescriptorSetProxy;
    using Self = DescriptorSetAllocator;
    using PoolState = details::PoolState;
    using CapacityMap = details::PoolCapacityMap;
    using PoolStates = std::flat_map<vk::DescriptorPool, PoolState>;
    struct PoolSetCompare
    {
        bool operator()(vk::DescriptorPool lhs, vk::DescriptorPool rhs) const noexcept;
        const PoolStates * m_pool_states_p = nullptr;
    };
    using PoolSet = std::flat_set<vk::DescriptorPool, PoolSetCompare>;
    using PoolSetMap = std::flat_map<VkDescriptorPoolCreateFlags, PoolSet>;
    using PendingAllocationQueue = std::deque<std::pair<DescriptorSetAllocation, ResourceLease>>;
public:
    ~DescriptorSetAllocator() noexcept;
    DescriptorSetAllocator() noexcept = default;
    DescriptorSetAllocator(const Self &) noexcept = delete;
    DescriptorSetAllocator(Self &&) noexcept;
    Self & operator=(const Self &) noexcept = delete;
    Self & operator=(Self &&) noexcept;
public:
    std::error_code create(vk::Device device, const DescriptorSetAllocatorInfo & info) noexcept;
    std::expected<DescriptorSetAllocation, std::error_code> allocate(const DescriptorSetLayout & layout, uint32_t count = 1u) noexcept;
    void recycle(const DescriptorSetAllocation & allocation, ResourceLease lease = {}) noexcept;
private:
    void deallocate(DescriptorSetAllocation allocation) noexcept;
    std::expected<std::vector<vk::DescriptorSet>, std::error_code> allocateFromPool(
        vk::DescriptorPool pool,
        std::span<const vk::DescriptorSetLayout> layouts) noexcept;
    std::expected<vk::DescriptorPool, std::error_code> createPool(
        VkDescriptorPoolCreateFlags pool_key,
        uint32_t set_count,
        const CapacityMap & required_capacity,
        PoolState & pool_state) noexcept;
private:
    vk::Device m_device;
    DescriptorSetAllocatorInfo m_allocator_info;
    PoolStates m_pool_states;
    PoolSetMap m_pool_sets;
    PendingAllocationQueue m_pending_allocations;
};


} // namespace lcf::vkc::dsp
