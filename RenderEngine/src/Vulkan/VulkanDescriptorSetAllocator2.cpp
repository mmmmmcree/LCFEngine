#include "Vulkan/VulkanDescriptorSetAllocator2.h"
#include "Vulkan/VulkanDescriptorSet2.h"
#include "Vulkan/VulkanDescriptorSetLayout2.h"
#include "Vulkan/VulkanContext.h"
#include <algorithm>
#include <ranges>
#include <cassert>

using namespace lcf::render;
using Strategy = vkenums::DescriptorSetStrategy;

// ============================================================================
//  PoolGroup
// ============================================================================

vk::DescriptorPool VulkanDescriptorSetAllocator2::PoolGroup::getCurrentAvailablePool() const noexcept
{
    return m_available_pools.empty() ? nullptr : m_available_pools.back();
}

void VulkanDescriptorSetAllocator2::PoolGroup::setCurrentAvailablePoolFull()
{
    if (not m_available_pools.empty()) {
        m_full_pools.emplace_back(m_available_pools.back());
        m_available_pools.pop_back();
    }
}

void VulkanDescriptorSetAllocator2::PoolGroup::resetAllocatedSets(vk::Device device)
{
    for (auto pool : m_full_pools) {
        device.resetDescriptorPool(pool);
        m_available_pools.emplace_back(pool);
    }
    m_full_pools.clear();
    for (auto pool : m_available_pools) {
        device.resetDescriptorPool(pool);
    }
}

void VulkanDescriptorSetAllocator2::PoolGroup::destroyPools(vk::Device device)
{
    for (auto pool : m_available_pools) { device.destroyDescriptorPool(pool); }
    for (auto pool : m_full_pools)      { device.destroyDescriptorPool(pool); }
    m_available_pools.clear();
    m_full_pools.clear();
}

// ============================================================================
//  VulkanDescriptorSetAllocator2
// ============================================================================

VulkanDescriptorSetAllocator2::~VulkanDescriptorSetAllocator2()
{
    if (not m_context_p) { return; }
    auto device = m_context_p->getDevice();
    for (auto & pg : m_pool_groups) {
        pg.destroyPools(device);
    }
    for (auto & [handle, pool] : m_bindless_pool_map) {
        device.destroyDescriptorPool(pool);
    }
    m_bindless_pool_map.clear();
}

void VulkanDescriptorSetAllocator2::create(VulkanContext * context_p)
{
    m_context_p = context_p;
}

VulkanDescriptorSet2 VulkanDescriptorSetAllocator2::allocate(const VulkanDescriptorSetLayout2 & layout)
{
    auto strategy = layout.getStrategy();
    assert(strategy != Strategy::eBindless && "Use allocateBindless() for eBindless strategy");

    auto pool = this->acquirePool(strategy);
    if (not pool) { return {}; }

    vk::DescriptorSetAllocateInfo alloc_info;
    alloc_info.setDescriptorPool(pool)
              .setSetLayouts(layout.getHandle());

    vk::DescriptorSet handle;
    try {
        handle = m_context_p->getDevice().allocateDescriptorSets(alloc_info).front();
    } catch (const vk::OutOfPoolMemoryError &) {
        getPoolGroup(strategy).setCurrentAvailablePoolFull();
        return this->allocate(layout);  // retry with fresh pool
    }

    return VulkanDescriptorSet2{handle, layout.getBindings(), strategy, layout.getIndex()};
}

VulkanDescriptorSet2 VulkanDescriptorSetAllocator2::allocateBindless(
    const VulkanDescriptorSetLayout2 & layout,
    uint32_t variable_count)
{
    assert(layout.getStrategy() == Strategy::eBindless && "allocateBindless() requires eBindless strategy");

    // Build pool sizes from layout bindings
    std::vector<vk::DescriptorPoolSize> pool_sizes;
    for (const auto & binding : layout.getBindings()) {
        uint32_t count = binding.hasFlag(vk::DescriptorBindingFlagBits::eVariableDescriptorCount)
                       ? variable_count
                       : binding.descriptorCount;
        pool_sizes.emplace_back(binding.descriptorType, count);
    }

    // Create eUpdateAfterBind pool with maxSets=1
    vk::DescriptorPoolCreateInfo pool_info;
    pool_info.setFlags(vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind)
             .setMaxSets(1u)
             .setPoolSizes(pool_sizes);

    vk::DescriptorPool pool;
    try {
        pool = m_context_p->getDevice().createDescriptorPool(pool_info);
    } catch (const vk::SystemError &) {
        return {};
    }

    // Allocate the single descriptor set from this pool
    vk::DescriptorSetAllocateInfo alloc_info;
    alloc_info.setDescriptorPool(pool)
              .setSetLayouts(layout.getHandle());

    vk::DescriptorSetVariableDescriptorCountAllocateInfo variable_count_info;
    bool has_variable = std::ranges::any_of(layout.getBindings(), [](const auto & b) {
        return b.hasFlag(vk::DescriptorBindingFlagBits::eVariableDescriptorCount);
    });

    if (has_variable) {
        variable_count_info.setDescriptorCounts(variable_count);
        alloc_info.setPNext(&variable_count_info);
    }

    vk::DescriptorSet set;
    try {
        set = m_context_p->getDevice().allocateDescriptorSets(alloc_info).front();
    } catch (const vk::SystemError &) {
        m_context_p->getDevice().destroyDescriptorPool(pool);
        return {};
    }

    // Track the pool by DS handle so deallocateBindless() can find it
    m_bindless_pool_map[static_cast<VkDescriptorSet>(set)] = pool;

    return VulkanDescriptorSet2{set, layout.getBindings(), Strategy::eBindless, layout.getIndex()};
}

void VulkanDescriptorSetAllocator2::deallocate(vk::DescriptorSet handle, vkenums::DescriptorSetStrategy strategy)
{
    if (not handle) { return; }
    switch (strategy) {
        case Strategy::ePerFrame:
            break;  // no-op — batch reset
        case Strategy::ePersistent:
            for (auto pool : m_pool_groups[1].m_available_pools) {
                m_context_p->getDevice().freeDescriptorSets(pool, handle);
                return;
            }
            break;
        case Strategy::eBindless:
            this->deallocateBindless(handle);
            break;
    }
}

void VulkanDescriptorSetAllocator2::deallocateBindless(vk::DescriptorSet handle)
{
    if (not handle || not m_context_p) { return; }
    auto it = m_bindless_pool_map.find(static_cast<VkDescriptorSet>(handle));
    if (it == m_bindless_pool_map.end()) { return; }
    m_context_p->getDevice().destroyDescriptorPool(it->second);
    m_bindless_pool_map.erase(it);
}

void VulkanDescriptorSetAllocator2::resetPools(vkenums::DescriptorSetStrategy strategy)
{
    if (not m_context_p) { return; }
    assert(strategy != Strategy::eBindless && "Cannot batch-reset bindless pools");
    getPoolGroup(strategy).resetAllocatedSets(m_context_p->getDevice());
}

vk::DescriptorPool VulkanDescriptorSetAllocator2::acquirePool(vkenums::DescriptorSetStrategy strategy)
{
    auto & pg = getPoolGroup(strategy);
    if (pg.m_available_pools.empty()) {
        if (auto pool = this->createPool(strategy)) {
            pg.m_available_pools.emplace_back(pool);
        }
    }
    return pg.getCurrentAvailablePool();
}

vk::DescriptorPool VulkanDescriptorSetAllocator2::createPool(vkenums::DescriptorSetStrategy strategy)
{
    static constexpr uint32_t k_sets_per_pool = 64u;

    vk::DescriptorPoolCreateInfo pool_info;
    pool_info.setMaxSets(k_sets_per_pool);

    if (strategy == Strategy::ePersistent) {
        static const std::vector<vk::DescriptorPoolSize> k_persistent_sizes = {
            { vk::DescriptorType::eUniformBuffer,        k_sets_per_pool * 2u },
            { vk::DescriptorType::eStorageBuffer,        k_sets_per_pool * 2u },
            { vk::DescriptorType::eCombinedImageSampler, k_sets_per_pool * 4u },
            { vk::DescriptorType::eSampledImage,         k_sets_per_pool * 4u },
            { vk::DescriptorType::eSampler,              k_sets_per_pool      },
        };
        pool_info.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
                 .setPoolSizes(k_persistent_sizes);
    } else {
        // ePerFrame — no special flags, batch reset
        static const std::vector<vk::DescriptorPoolSize> k_per_frame_sizes = {
            { vk::DescriptorType::eUniformBuffer,        k_sets_per_pool * 2u },
            { vk::DescriptorType::eUniformBufferDynamic, k_sets_per_pool * 2u },
            { vk::DescriptorType::eStorageBuffer,        k_sets_per_pool      },
            { vk::DescriptorType::eCombinedImageSampler, k_sets_per_pool * 2u },
            { vk::DescriptorType::eSampledImage,         k_sets_per_pool * 2u },
            { vk::DescriptorType::eSampler,              k_sets_per_pool      },
        };
        pool_info.setPoolSizes(k_per_frame_sizes);
    }

    vk::DescriptorPool pool;
    try {
        pool = m_context_p->getDevice().createDescriptorPool(pool_info);
    } catch (const vk::SystemError &) {}
    return pool;
}

VulkanDescriptorSetAllocator2::PoolGroup & VulkanDescriptorSetAllocator2::getPoolGroup(
    vkenums::DescriptorSetStrategy strategy)
{
    // [0] = ePerFrame, [1] = ePersistent
    return m_pool_groups[strategy == Strategy::ePersistent ? 1 : 0];
}
