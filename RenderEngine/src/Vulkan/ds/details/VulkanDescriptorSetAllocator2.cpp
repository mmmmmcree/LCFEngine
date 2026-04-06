#include "Vulkan/ds/details/VulkanDescriptorSetAllocator2.h"
#include "Vulkan/ds/VulkanDescriptorSet2.h"
#include "Vulkan/ds/VulkanDescriptorSetLayout2.h"
#include "Vulkan/vulkan_constants.h"
#include <ranges>

using namespace lcf::render;
using namespace lcf::render::detail;

VulkanDescriptorSetAllocator2::~VulkanDescriptorSetAllocator2() noexcept
{
    for (auto & [_, pool_group] : m_pool_groups) {
        const_cast<PoolGroup &>(pool_group).destroyPools(m_device);
    }
    for (auto [_, pool] : m_set_to_pool_map[vkenums::DescriptorSetStrategy::eBindless]) {
        m_device.destroyDescriptorPool(pool);
    }
}

void VulkanDescriptorSetAllocator2::create(vk::Device device) noexcept
{
    m_device = device;
}

VulkanDescriptorSetAllocator2::AllocResult VulkanDescriptorSetAllocator2::allocate(const VulkanDescriptorSetLayout2 & layout) noexcept
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

VulkanDescriptorSetAllocator2::AllocResult VulkanDescriptorSetAllocator2::allocateBindless(
    const VulkanDescriptorSetLayout2 & layout,
    uint32_t variable_count) noexcept
{
    auto strategy = layout.getStrategy();
    if (strategy != vkenums::DescriptorSetStrategy::eBindless) {
        return std::unexpected(std::make_error_code(std::errc::invalid_argument));
    }
    auto pool = this->createPoolForBindless(layout, variable_count);
    vk::DescriptorSetAllocateInfo alloc_info;
    vk::DescriptorSetVariableDescriptorCountAllocateInfo variable_descriptor_count_info;
    variable_descriptor_count_info.setDescriptorCounts(variable_count);
    alloc_info.setDescriptorPool(pool)
        .setSetLayouts(layout.getHandle())
        .setPNext(&variable_descriptor_count_info);
    return this->allocate(layout, alloc_info);
}

void VulkanDescriptorSetAllocator2::deallocate(VulkanDescriptorSet2 && set)
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

VulkanDescriptorSetAllocator2::AllocResult VulkanDescriptorSetAllocator2::allocate(
    const VulkanDescriptorSetLayout2 & layout,
    const vk::DescriptorSetAllocateInfo & alloc_info) noexcept
{
    vk::DescriptorSet descriptor_set;
    try {
        descriptor_set = m_device.allocateDescriptorSets(alloc_info).front();
    } catch (const vk::SystemError & e) {
        return std::unexpected(e.code());
    }
    m_set_to_pool_map[layout.getStrategy()][descriptor_set] = alloc_info.descriptorPool;
    return VulkanDescriptorSet2 { descriptor_set, layout.getBindings(), layout.getStrategy(), layout.getIndex() };
}

vk::DescriptorPool VulkanDescriptorSetAllocator2::tryGetPool(vkenums::DescriptorSetStrategy strategy) noexcept
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

vk::DescriptorPool VulkanDescriptorSetAllocator2::createPool(vkenums::DescriptorSetStrategy strategy) noexcept
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

vk::DescriptorPool VulkanDescriptorSetAllocator2::createPoolForBindless(
    const VulkanDescriptorSetLayout2 & layout,
    uint32_t variable_count) noexcept
{
    std::vector<vk::DescriptorPoolSize> pool_sizes;
    for (const auto & binding : layout.getBindings()) {
        pool_sizes.emplace_back(binding.getDescriptorType(), binding.getDescriptorCount());
    }
    if (not layout.getBindings().empty()) {
        const auto & last_binding = layout.getBindings().back();
        if (last_binding.containsFlags(vk::DescriptorBindingFlagBits::eVariableDescriptorCount)) {
            pool_sizes.back().descriptorCount = variable_count;
        }
    }
    vk::DescriptorPoolCreateInfo pool_info;
    pool_info.setFlags(vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind)
        .setMaxSets(1)
        .setPoolSizes(pool_sizes);
    vk::DescriptorPool pool {};
    try {
        pool = m_device.createDescriptorPool(pool_info);
    } catch (const vk::SystemError &) {}
    return pool;
}

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

void VulkanDescriptorSetAllocator2::PoolGroup::destroyCurrentAvailablePool(vk::Device device)
{
    if (not m_available_pools.empty()) {
        device.destroyDescriptorPool(m_available_pools.back());
        m_available_pools.pop_back();
    }
}

void VulkanDescriptorSetAllocator2::PoolGroup::destroyPools(vk::Device device) noexcept
{
    for (auto pool : m_available_pools) { device.destroyDescriptorPool(pool); }
    for (auto pool : m_full_pools) { device.destroyDescriptorPool(pool); }
    m_available_pools.clear();
    m_full_pools.clear();
}

void VulkanDescriptorSetAllocator2::PoolGroup::addPool(vk::DescriptorPool pool)
{
    m_available_pools.emplace_back(pool);
}
