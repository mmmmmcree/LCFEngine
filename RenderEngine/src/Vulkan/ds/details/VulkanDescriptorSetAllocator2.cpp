#include "Vulkan/ds/details/VulkanDescriptorSetAllocator.h"
#include "Vulkan/ds/VulkanDescriptorSet.h"
#include "Vulkan/ds/VulkanDescriptorSetLayout.h"
#include "Vulkan/vulkan_constants.h"
#include <ranges>

using namespace lcf::render;
using namespace lcf::render::detail;

VulkanDescriptorSetAllocator::~VulkanDescriptorSetAllocator() noexcept
{
    for (auto & [_, pool_group] : m_pool_groups) {
        const_cast<PoolGroup &>(pool_group).destroyPools(m_device);
    }
    for (auto [_, pool] : m_set_to_pool_map[vkenums::DescriptorSetStrategy::eBindless]) {
        m_device.destroyDescriptorPool(pool);
    }
}

std::error_code VulkanDescriptorSetAllocator::create(vk::Device device) noexcept
{
    if (not device) { return std::make_error_code(std::errc::invalid_argument);  }
    m_device = device;
    return {};
}

VulkanDescriptorSetAllocator::AllocResult VulkanDescriptorSetAllocator::allocate(const VulkanDescriptorSetLayout & layout) noexcept
{
    auto strategy = layout.getStrategy();
    if (strategy == vkenums::DescriptorSetStrategy::eBindless) {
        return std::unexpected(std::make_error_code(std::errc::invalid_argument));
    }
    auto pool = this->tryGetPool(strategy);
    vk::DescriptorSetAllocateInfo alloc_info;
    alloc_info.setDescriptorPool(pool)
        .setSetLayouts(layout.getHandle());
    return this->allocate(layout, alloc_info);
}

void VulkanDescriptorSetAllocator::deallocate(VulkanDescriptorSet && set)
{
    auto pool = m_set_to_pool_map[set.getStrategy()][set.getHandle()];
    switch (set.getStrategy()) {
        case vkenums::DescriptorSetStrategy::eIndividual: {
            m_device.freeDescriptorSets(pool, set.getHandle());
        } break;
        case vkenums::DescriptorSetStrategy::eBindless: {
            m_device.destroyDescriptorPool(pool);
        } break;
    }
    m_set_to_pool_map[set.getStrategy()].erase(set.getHandle());
}

VulkanDescriptorSetAllocator::AllocResult VulkanDescriptorSetAllocator::allocate(
    const VulkanDescriptorSetLayout & layout,
    const vk::DescriptorSetAllocateInfo & alloc_info) noexcept
{
    vk::DescriptorSet descriptor_set;
    try {
        descriptor_set = m_device.allocateDescriptorSets(alloc_info).front();
    } catch (const vk::SystemError & e) {
        return std::unexpected(e.code());
    }
    m_set_to_pool_map[layout.getStrategy()][descriptor_set] = alloc_info.descriptorPool;
    return VulkanDescriptorSet { descriptor_set, layout.getBindings(), layout.getStrategy(), layout.getIndex() };
}

vk::DescriptorPool VulkanDescriptorSetAllocator::tryGetPool(vkenums::DescriptorSetStrategy strategy) noexcept
{
    auto & pool_group = m_pool_groups[strategy];
    auto pool = pool_group.getCurrentAvailablePool();
    if (not pool) {
        pool = this->createPool(strategy);
        if (not pool) { return nullptr; }
        pool_group.addPool(pool);
    }
    return pool;
}

vk::DescriptorPool VulkanDescriptorSetAllocator::createPool(vkenums::DescriptorSetStrategy strategy) noexcept
{
    vk::DescriptorPoolCreateInfo pool_info;
    pool_info.setMaxSets(vkconstants::ds::k_max_sets_per_pool);

    if (strategy == vkenums::DescriptorSetStrategy::eIndividual) {
        static constexpr vk::DescriptorPoolSize k_pool_sizes[]
        {
            { vk::DescriptorType::eSampler, static_cast<uint32_t>(vkconstants::ds::k_max_sets_per_pool * 0.5f) },
            { vk::DescriptorType::eCombinedImageSampler, static_cast<uint32_t>(vkconstants::ds::k_max_sets_per_pool * 4.f) },
            { vk::DescriptorType::eSampledImage, static_cast<uint32_t>(vkconstants::ds::k_max_sets_per_pool * 4.f) },
            { vk::DescriptorType::eStorageImage, static_cast<uint32_t>(vkconstants::ds::k_max_sets_per_pool * 1.f) },
            { vk::DescriptorType::eUniformTexelBuffer, static_cast<uint32_t>(vkconstants::ds::k_max_sets_per_pool * 1.f) },
            { vk::DescriptorType::eStorageTexelBuffer, static_cast<uint32_t>(vkconstants::ds::k_max_sets_per_pool * 1.f) },
            { vk::DescriptorType::eUniformBuffer, static_cast<uint32_t>(vkconstants::ds::k_max_sets_per_pool * 2.f) },
            { vk::DescriptorType::eStorageBuffer, static_cast<uint32_t>(vkconstants::ds::k_max_sets_per_pool * 2.f) },
            { vk::DescriptorType::eUniformBufferDynamic, static_cast<uint32_t>(vkconstants::ds::k_max_sets_per_pool * 1.f) },
            { vk::DescriptorType::eStorageBufferDynamic, static_cast<uint32_t>(vkconstants::ds::k_max_sets_per_pool * 1.f) },
            { vk::DescriptorType::eInputAttachment, static_cast<uint32_t>(vkconstants::ds::k_max_sets_per_pool * 0.5f) }
        };
        pool_info.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
            .setPoolSizes(k_pool_sizes);
    }
    vk::DescriptorPool pool;
    try {
        pool = m_device.createDescriptorPool(pool_info);
    } catch (const vk::SystemError &) {}
    return pool;
}

// ============================================================================
//  PoolGroup
// ============================================================================

vk::DescriptorPool VulkanDescriptorSetAllocator::PoolGroup::getCurrentAvailablePool() const noexcept
{
    return m_available_pools.empty() ? nullptr : m_available_pools.back();
}

void VulkanDescriptorSetAllocator::PoolGroup::setCurrentAvailablePoolFull()
{
    if (not m_available_pools.empty()) {
        m_full_pools.emplace_back(m_available_pools.back());
        m_available_pools.pop_back();
    }
}

void VulkanDescriptorSetAllocator::PoolGroup::destroyCurrentAvailablePool(vk::Device device)
{
    if (not m_available_pools.empty()) {
        device.destroyDescriptorPool(m_available_pools.back());
        m_available_pools.pop_back();
    }
}

void VulkanDescriptorSetAllocator::PoolGroup::destroyPools(vk::Device device) noexcept
{
    for (auto pool : m_available_pools) { device.destroyDescriptorPool(pool); }
    for (auto pool : m_full_pools) { device.destroyDescriptorPool(pool); }
    m_available_pools.clear();
    m_full_pools.clear();
}

void VulkanDescriptorSetAllocator::PoolGroup::addPool(vk::DescriptorPool pool)
{
    m_available_pools.emplace_back(pool);
}
