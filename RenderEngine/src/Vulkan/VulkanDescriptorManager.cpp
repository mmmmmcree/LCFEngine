#include "VulkanDescriptorManager.h"
#include "VulkanContext.h"

using namespace lcf::render;

void lcf::render::VulkanDescriptorManager::create(VulkanContext * context_p)
{
    m_context_p = context_p;
}

lcf::render::VulkanDescriptorManager::~VulkanDescriptorManager()
{
    this->resetAllocatedSets();
    auto device = m_context_p->getDevice();
    for (auto & [_, pool_group] : m_pool_group_map) {
        pool_group.destroyPools(device);
    }
}

vk::DescriptorSet VulkanDescriptorManager::allocate(vk::DescriptorSetLayout layout)
{
    auto alloc_result = this->allocate({&layout, 1});
    if (alloc_result.empty()) { return nullptr; }
    return alloc_result.front();
}

VulkanDescriptorManager::DescriptorSetList VulkanDescriptorManager::allocate(std::span<const vk::DescriptorSetLayout> layouts)
{
    auto pool = this->tryGetPool({});
    if (not pool) { return {}; }
    auto device = m_context_p->getDevice();
    vk::DescriptorSetAllocateInfo alloc_info;
    alloc_info.setDescriptorPool(pool).setSetLayouts(layouts);
    DescriptorSetList descriptor_sets;
    try {
        descriptor_sets = device.allocateDescriptorSets(alloc_info);
    } catch (const vk::OutOfPoolMemoryError &e) {
        this->getPoolGroup({}).setCurrentAvailablePoolFull();
        return this->allocate(layouts);
    }
    return descriptor_sets;
}

vk::UniqueDescriptorSet lcf::render::VulkanDescriptorManager::allocateUnique(vk::DescriptorSetLayout layout)
{
    auto alloc_result = this->allocateUnique({&layout, 1});
    if (alloc_result.empty()) { return {}; }
    return std::move(alloc_result.front());
}

VulkanDescriptorManager::UniqueDescriptorSetList VulkanDescriptorManager::allocateUnique(std::span<const vk::DescriptorSetLayout> layouts)
{
    auto pool = this->tryGetPool(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet);
    if (not pool) { return {}; }
    auto device = m_context_p->getDevice();
    vk::DescriptorSetAllocateInfo alloc_info;
    alloc_info.setDescriptorPool(pool).setSetLayouts(layouts);
    UniqueDescriptorSetList descriptor_sets;
    try {
        descriptor_sets = device.allocateDescriptorSetsUnique(alloc_info);
    } catch (const vk::OutOfPoolMemoryError &e) {
        this->getPoolGroup(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet).setCurrentAvailablePoolFull();
        return this->allocateUnique(layouts);
    }
    return descriptor_sets;
}

void lcf::render::VulkanDescriptorManager::resetAllocatedSets()
{
    auto device = m_context_p->getDevice();
    auto & [full_pools, available_pools] = this->getPoolGroup({});
    available_pools.insert(available_pools.end(), full_pools.begin(), full_pools.end());
    for (auto &pool : available_pools) {
        device.resetDescriptorPool(pool);
    }
    full_pools.clear();
}

vk::DescriptorPool lcf::render::VulkanDescriptorManager::createNewPool(vk::DescriptorPoolCreateFlags flags)
{
    //todo temporary solution, create a pool with all types of descriptors
    static constexpr uint32_t SETS_PER_POOL = 100;
    static std::vector<vk::DescriptorPoolSize> pool_sizes = {
        { vk::DescriptorType::eSampler, static_cast<uint32_t>(SETS_PER_POOL * 0.5f) },
        { vk::DescriptorType::eCombinedImageSampler, static_cast<uint32_t>(SETS_PER_POOL * 4.f) },
        { vk::DescriptorType::eSampledImage, static_cast<uint32_t>(SETS_PER_POOL * 4.f) },
        { vk::DescriptorType::eStorageImage, static_cast<uint32_t>(SETS_PER_POOL * 1.f) },
        { vk::DescriptorType::eUniformTexelBuffer, static_cast<uint32_t>(SETS_PER_POOL * 1.f) },
        { vk::DescriptorType::eStorageTexelBuffer, static_cast<uint32_t>(SETS_PER_POOL * 1.f) },
        { vk::DescriptorType::eUniformBuffer, static_cast<uint32_t>(SETS_PER_POOL * 2.f) },
        { vk::DescriptorType::eStorageBuffer, static_cast<uint32_t>(SETS_PER_POOL * 2.f) },
        { vk::DescriptorType::eUniformBufferDynamic, static_cast<uint32_t>(SETS_PER_POOL * 1.f) },
        { vk::DescriptorType::eStorageBufferDynamic, static_cast<uint32_t>(SETS_PER_POOL * 1.f) },
        { vk::DescriptorType::eInputAttachment, static_cast<uint32_t>(SETS_PER_POOL * 0.5f) }
    };
    //todo -------------------------------------------------------------------------
    auto device = m_context_p->getDevice();
    vk::DescriptorPoolCreateInfo pool_info;
    pool_info.setFlags(flags)
        .setMaxSets(SETS_PER_POOL)
        .setPoolSizes(pool_sizes);
    vk::DescriptorPool pool;
    try {
        pool = device.createDescriptorPool(pool_info);
    } catch (const vk::SystemError &e) { }
    if (pool) { this->getPoolGroup(flags).m_available_pools.emplace_back(pool); }
    return pool;
}

vk::DescriptorPool lcf::render::VulkanDescriptorManager::tryGetPool(vk::DescriptorPoolCreateFlags flags)
{
    auto pool = this->getPoolGroup(flags).getCurrentAvailablePool();
    if (not pool) { return this->createNewPool(flags); }
    return pool;
}

VulkanDescriptorManager::PoolGroup & VulkanDescriptorManager::getPoolGroup(vk::DescriptorPoolCreateFlags flags)
{
    using UnderlyingType = vk::DescriptorPoolCreateFlags::MaskType;
    return m_pool_group_map[static_cast<UnderlyingType>(flags)];
}

vk::DescriptorPool VulkanDescriptorManager::PoolGroup::getCurrentAvailablePool() const noexcept
{
    return m_available_pools.empty() ? nullptr : m_available_pools.back();
}

void VulkanDescriptorManager::PoolGroup::setCurrentAvailablePoolFull()
{
    auto pool = this->getCurrentAvailablePool();
    if (not pool) { return; }
    m_full_pools.emplace_back(pool);
    m_available_pools.pop_back();
}

void VulkanDescriptorManager::PoolGroup::destroyPools(vk::Device device)
{
    for (auto &pool : m_full_pools) {
        device.destroyDescriptorPool(pool);
    }
    for (auto &pool : m_available_pools) {
        device.destroyDescriptorPool(pool);
    }
    m_full_pools.clear();
    m_available_pools.clear();
}
