#include "Vulkan/BindlessPoolManager.h"
#include "Vulkan/VulkanDescriptorSetLayout2.h"
#include "Vulkan/VulkanContext.h"

using namespace lcf::render;

BindlessPoolManager::~BindlessPoolManager()
{
    if (not m_context_p) { return; }
    auto device = m_context_p->getDevice();
    if (m_pool)          { device.destroyDescriptorPool(m_pool); }
    if (m_incoming_pool) { device.destroyDescriptorPool(m_incoming_pool); }
    for (auto & retired : m_retired_pools) { device.destroyDescriptorPool(retired.pool); }
}

void BindlessPoolManager::create(VulkanContext * context_p)
{
    m_context_p = context_p;
}

VulkanDescriptorSet2 BindlessPoolManager::allocate(const VulkanDescriptorSetLayout2 & layout,
                                                    uint32_t initial_variable_count)
{
    m_layout_p = &layout;

    if (not m_pool) {
        m_pool = this->createPool(initial_variable_count, layout);
        if (not m_pool) { return {}; }
        m_descriptor_set   = this->allocateFromPool(m_pool, layout, initial_variable_count);
        m_current_capacity = initial_variable_count;
    }

    VulkanDescriptorSet2 ds;
    ds.m_descriptor_set = m_descriptor_set;
    ds.m_binding_list.assign(layout.getBindings().begin(), layout.getBindings().end());
    ds.m_strategy  = layout.getStrategy();
    ds.m_set_index = layout.getIndex();
    return ds;
}

void BindlessPoolManager::beginFrame(uint32_t frame_index, uint32_t frames_in_flight)
{
    auto device = m_context_p->getDevice();
    std::erase_if(m_retired_pools, [&](const RetiredPool & rp) {
        if (frame_index >= rp.retire_frame) {
            device.destroyDescriptorPool(rp.pool);
            return true;
        }
        return false;
    });

    if (m_growth_state == GrowthState::eBuilding) {
        m_growth_state = GrowthState::eSwapPending;
    } else if (m_growth_state == GrowthState::eSwapPending) {
        m_retired_pools.push_back({ m_pool, frame_index + frames_in_flight });
        m_pool             = m_incoming_pool;
        m_descriptor_set   = m_incoming_set;
        m_current_capacity = m_incoming_capacity;
        m_incoming_pool     = nullptr;
        m_incoming_set      = nullptr;
        m_incoming_capacity = 0u;
        m_growth_state      = GrowthState::eIdle;
    }
}

void BindlessPoolManager::triggerGrowth(const VulkanDescriptorSetLayout2 & layout)
{
    if (m_growth_state != GrowthState::eIdle) { return; }
    uint32_t new_capacity = m_current_capacity * 2u;
    m_incoming_pool = this->createPool(new_capacity, layout);
    if (not m_incoming_pool) { return; }
    m_incoming_set      = this->allocateFromPool(m_incoming_pool, layout, new_capacity);
    m_incoming_capacity = new_capacity;
    m_growth_state      = GrowthState::eBuilding;
}

vk::DescriptorPool BindlessPoolManager::createPool(uint32_t variable_count,
                                                    const VulkanDescriptorSetLayout2 & layout)
{
    std::vector<vk::DescriptorPoolSize> pool_sizes;
    for (const auto & b : layout.getBindings()) {
        uint32_t count = b.is_runtime_array ? variable_count : b.descriptorCount;
        pool_sizes.push_back({ b.descriptorType, count });
    }

    vk::DescriptorPoolCreateInfo pool_info;
    pool_info.setFlags(vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind)
             .setMaxSets(1u)
             .setPoolSizes(pool_sizes);

    vk::DescriptorPool pool;
    try {
        pool = m_context_p->getDevice().createDescriptorPool(pool_info);
    } catch (const vk::SystemError &) {}
    return pool;
}

vk::DescriptorSet BindlessPoolManager::allocateFromPool(vk::DescriptorPool pool,
                                                         const VulkanDescriptorSetLayout2 & layout,
                                                         uint32_t variable_count)
{
    vk::DescriptorSetAllocateInfo alloc_info;
    alloc_info.setDescriptorPool(pool)
              .setSetLayouts(layout.getHandle());

    vk::DescriptorSetVariableDescriptorCountAllocateInfo variable_count_info;
    bool has_runtime_array = std::ranges::any_of(layout.getBindings(),
        [](const auto & b) { return b.is_runtime_array; });

    if (has_runtime_array) {
        variable_count_info.setDescriptorCounts(variable_count);
        alloc_info.setPNext(&variable_count_info);
    }

    try {
        return m_context_p->getDevice().allocateDescriptorSets(alloc_info).front();
    } catch (const vk::SystemError &) {}
    return nullptr;
}
