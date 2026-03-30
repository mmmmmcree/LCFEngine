#include "Vulkan/PersistentPoolManager.h"
#include "Vulkan/VulkanDescriptorSetLayout2.h"
#include "Vulkan/VulkanContext.h"

using namespace lcf::render;

PersistentPoolManager::~PersistentPoolManager()
{
    if (not m_context_p) { return; }
    auto device = m_context_p->getDevice();
    for (auto pool : m_available_pools) { device.destroyDescriptorPool(pool); }
    for (auto pool : m_full_pools)      { device.destroyDescriptorPool(pool); }
}

void PersistentPoolManager::create(VulkanContext * context_p)
{
    m_context_p = context_p;
}

VulkanDescriptorSet2 PersistentPoolManager::allocate(const VulkanDescriptorSetLayout2 & layout)
{
    auto pool = this->acquirePool();
    if (not pool) { return {}; }

    vk::DescriptorSetAllocateInfo alloc_info;
    alloc_info.setDescriptorPool(pool)
              .setSetLayouts(layout.getHandle());

    vk::DescriptorSet handle;
    try {
        handle = m_context_p->getDevice().allocateDescriptorSets(alloc_info).front();
    } catch (const vk::OutOfPoolMemoryError &) {
        m_full_pools.emplace_back(m_available_pools.back());
        m_available_pools.pop_back();
        return this->allocate(layout);
    }

    VulkanDescriptorSet2 ds;
    ds.m_descriptor_set = handle;
    ds.m_binding_list.assign(layout.getBindings().begin(), layout.getBindings().end());
    ds.m_strategy  = layout.getStrategy();
    ds.m_set_index = layout.getIndex();
    return ds;
}

void PersistentPoolManager::free(vk::DescriptorSet descriptor_set)
{
    if (not descriptor_set) { return; }
    for (auto pool : m_available_pools) {
        m_context_p->getDevice().freeDescriptorSets(pool, descriptor_set);
        return;
    }
}

vk::DescriptorPool PersistentPoolManager::acquirePool()
{
    if (m_available_pools.empty()) {
        if (auto pool = this->createPool()) {
            m_available_pools.emplace_back(pool);
        }
    }
    return m_available_pools.empty() ? nullptr : m_available_pools.back();
}

vk::DescriptorPool PersistentPoolManager::createPool()
{
    static constexpr uint32_t kSetsPerPool = 64u;
    static const std::vector<vk::DescriptorPoolSize> kPoolSizes = {
        { vk::DescriptorType::eUniformBuffer,        kSetsPerPool * 2u },
        { vk::DescriptorType::eStorageBuffer,        kSetsPerPool * 2u },
        { vk::DescriptorType::eCombinedImageSampler, kSetsPerPool * 4u },
        { vk::DescriptorType::eSampledImage,         kSetsPerPool * 4u },
        { vk::DescriptorType::eSampler,              kSetsPerPool      },
    };

    vk::DescriptorPoolCreateInfo pool_info;
    pool_info.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
             .setMaxSets(kSetsPerPool)
             .setPoolSizes(kPoolSizes);

    vk::DescriptorPool pool;
    try {
        pool = m_context_p->getDevice().createDescriptorPool(pool_info);
    } catch (const vk::SystemError &) {}
    return pool;
}
