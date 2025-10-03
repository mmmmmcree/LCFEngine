#include "VulkanDescriptorManager.h"
#include "VulkanContext.h"


void lcf::render::VulkanDescriptorManager::create(VulkanContext * context)
{
    m_context_p = context;
}

lcf::render::VulkanDescriptorManager::~VulkanDescriptorManager()
{
    this->resetAllocatedSets();
    auto device = m_context_p->getDevice();
    for (auto &pool : m_full_pools) {
        device.destroyDescriptorPool(pool); 
    }
    for (auto &pool : m_available_pools) {
        device.destroyDescriptorPool(pool); 
    }
}

std::optional<vk::DescriptorSet> lcf::render::VulkanDescriptorManager::allocate(const vk::DescriptorSetLayout &layout)
{
    auto alloc_result = this->allocate({&layout, 1});
    if (not alloc_result) { return std::nullopt; }
    return alloc_result.value().front();
}

std::optional<lcf::render::VulkanDescriptorManager::DescriptorSetList> lcf::render::VulkanDescriptorManager::allocate(std::span<const vk::DescriptorSetLayout> layouts)
{
    DescriptorSetList descriptor_sets;
    if (m_available_pools.empty()) { this->createNewPool(); }
    if (m_available_pools.empty()) { return std::nullopt; }
    auto device = m_context_p->getDevice();
    auto pool = m_available_pools.back();
    vk::DescriptorSetAllocateInfo alloc_info;
    alloc_info.setDescriptorPool(pool).setSetLayouts(layouts);
    try {
        descriptor_sets = device.allocateDescriptorSets(alloc_info);
    } catch (const vk::OutOfPoolMemoryError &e) {
        m_full_pools.push_back(pool);
        m_available_pools.pop_back();
        this->createNewPool();
        if (m_available_pools.empty()) { return std::nullopt; }
        pool = m_available_pools.back();
        alloc_info.setDescriptorPool(pool);
        try {
            descriptor_sets = device.allocateDescriptorSets(alloc_info);
        } catch (const vk::SystemError &e) {
            qDebug() << "VulkanDescriptorManager::allocate() failed: " << e.what();
            return std::nullopt;
        }
    }
    return descriptor_sets;
}

void lcf::render::VulkanDescriptorManager::resetAllocatedSets()
{
    auto device = m_context_p->getDevice();
    m_available_pools.insert(m_available_pools.end(), m_full_pools.begin(), m_full_pools.end());
    for (auto &pool : m_available_pools) {
        device.resetDescriptorPool(pool);
    }
    m_full_pools.clear();
}

void lcf::render::VulkanDescriptorManager::createNewPool()
{
    //todo temporary solution, create a pool with all types of descriptors
    static constexpr uint32_t SETS_PER_POOL = 1000;
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
    pool_info.setMaxSets(SETS_PER_POOL) .setPoolSizes(pool_sizes);
    vk::DescriptorPool pool;
    try {
        pool = device.createDescriptorPool(pool_info);
    } catch (const vk::SystemError &e) { }
    if (pool) { m_available_pools.emplace_back(pool); }
}