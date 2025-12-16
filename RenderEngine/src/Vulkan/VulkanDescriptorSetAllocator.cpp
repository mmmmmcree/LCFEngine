#include "vulkan/VulkanDescriptorSetAllocator.h"
#include "vulkan/VulkanContext.h"

using namespace lcf::render;

VulkanDescriptorSetAllocator::~VulkanDescriptorSetAllocator()
{
    for (auto & [flags_value, pool_group] : m_pool_group_map) {
        vk::DescriptorPoolCreateFlags flags {flags_value};
        if (not (flags & vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)) {
            pool_group.resetAllocatedSets(m_context_p);
        }
        pool_group.destroyPools(m_context_p);
    }
}

void VulkanDescriptorSetAllocator::create(VulkanContext *context_p)
{
    m_context_p = context_p;
}

void VulkanDescriptorSetAllocator::allocate(VulkanDescriptorSet & descriptor_set) const
{
    vk::DescriptorPoolCreateFlags pool_flags;
    auto & layout_sp = descriptor_set.m_layout_sp;
    if (layout_sp->containsFlags(vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool)) {
        pool_flags |= vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind;
    } else {
        pool_flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    }
    auto pool = this->tryGetPool(pool_flags);
    if (not pool) { return; }
    auto device = m_context_p->getDevice();
    vk::DescriptorSetAllocateInfo alloc_info;
    alloc_info.setDescriptorPool(pool).setSetLayouts(layout_sp->getHandle());
    vk::DescriptorSetVariableDescriptorCountAllocateInfo variable_descriptor_count_info;
    uint32_t variable_descriptor_count = 4;
    variable_descriptor_count_info.setDescriptorCounts(variable_descriptor_count);
    if (pool_flags & vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind) {
        alloc_info.setPNext(&variable_descriptor_count_info);
    } 
    if (pool_flags & vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet) {
        descriptor_set.m_descriptor_pool_opt = pool;
    }
    try {
        descriptor_set.m_descriptor_set = device.allocateDescriptorSets(alloc_info).front();
    } catch (const vk::OutOfPoolMemoryError &e) {
        this->getPoolGroup(pool_flags).setCurrentAvailablePoolFull();
        this->allocate(descriptor_set);
    }
}

void VulkanDescriptorSetAllocator::resetAllocatedSets(vk::DescriptorPoolCreateFlags pool_flags) const
{
    if (pool_flags & vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet) { return; }
    this->getPoolGroup(pool_flags).resetAllocatedSets(m_context_p);
}

vk::DescriptorPool VulkanDescriptorSetAllocator::tryGetPool(vk::DescriptorPoolCreateFlags flags) const
{
    auto pool = this->getPoolGroup(flags).getCurrentAvailablePool();
    if (not pool) { return this->createNewPool(flags); }
    return pool;
}

vk::DescriptorPool VulkanDescriptorSetAllocator::createNewPool(vk::DescriptorPoolCreateFlags flags) const
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

VulkanDescriptorSetAllocator::PoolGroup & VulkanDescriptorSetAllocator::getPoolGroup(vk::DescriptorPoolCreateFlags flags) const
{
    using UnderlyingType = vk::DescriptorPoolCreateFlags::MaskType;
    return m_pool_group_map[static_cast<UnderlyingType>(flags)];
}

vk::DescriptorPool VulkanDescriptorSetAllocator::PoolGroup::getCurrentAvailablePool() const noexcept
{
    return m_available_pools.empty() ? nullptr : m_available_pools.back();
}

void VulkanDescriptorSetAllocator::PoolGroup::setCurrentAvailablePoolFull()
{
    auto pool = this->getCurrentAvailablePool();
    if (not pool) { return; }
    m_full_pools.emplace_back(pool);
    m_available_pools.pop_back();
}

void VulkanDescriptorSetAllocator::PoolGroup::resetAllocatedSets(VulkanContext * context_p)
{
    auto device = context_p->getDevice();
    for (auto & pool : m_full_pools) {
        m_available_pools.emplace_back(pool);
    }
    for (auto & pool : m_available_pools) {
        device.resetDescriptorPool(pool);
    }
    m_full_pools.clear();
}

void VulkanDescriptorSetAllocator::PoolGroup::destroyPools(VulkanContext * context_p)
{
    auto device = context_p->getDevice();
    for (auto &pool : m_full_pools) {
        device.destroyDescriptorPool(pool);
    }
    for (auto &pool : m_available_pools) {
        device.destroyDescriptorPool(pool);
    }
    m_full_pools.clear();
    m_available_pools.clear();
}