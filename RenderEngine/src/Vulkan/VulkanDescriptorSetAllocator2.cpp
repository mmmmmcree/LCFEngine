#include "Vulkan/VulkanDescriptorSetAllocator2.h"
#include "Vulkan/VulkanDescriptorSet2.h"
#include "Vulkan/VulkanDescriptorSetLayout2.h"
#include "Vulkan/VulkanContext.h"
#include "Vulkan/vulkan_constants.h"
#include <algorithm>
#include <ranges>

using namespace lcf::render;
using namespace lcf::render::detail;
using Strategy = vkenums::DescriptorSetStrategy;

// ============================================================================
//  detail::PerFrameDescriptorSetAllocator
// ============================================================================

PerFrameDescriptorSetAllocator::~PerFrameDescriptorSetAllocator()
{
    this->destroy();
}

void PerFrameDescriptorSetAllocator::create(VulkanContext * context_p)
{
    m_context_p = context_p;
}

void PerFrameDescriptorSetAllocator::destroy()
{
    if (not m_context_p) { return; }
    auto device = m_context_p->getDevice();
    for (auto pool : m_available_pools) { device.destroyDescriptorPool(pool); }
    for (auto pool : m_full_pools)      { device.destroyDescriptorPool(pool); }
    m_available_pools.clear();
    m_full_pools.clear();
}

VulkanDescriptorSet2 PerFrameDescriptorSetAllocator::allocate(const VulkanDescriptorSetLayout2 & layout)
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

    return VulkanDescriptorSet2{m_context_p, handle, layout.getBindings(), layout.getStrategy(), layout.getIndex()};
}

void PerFrameDescriptorSetAllocator::resetForFrame()
{
    if (not m_context_p) { return; }
    auto device = m_context_p->getDevice();
    for (auto pool : m_full_pools) {
        device.resetDescriptorPool(pool);
        m_available_pools.emplace_back(pool);
    }
    m_full_pools.clear();
    for (auto pool : m_available_pools) {
        device.resetDescriptorPool(pool);
    }
}

vk::DescriptorPool PerFrameDescriptorSetAllocator::acquirePool()
{
    if (m_available_pools.empty()) {
        if (auto pool = this->createPool()) {
            m_available_pools.emplace_back(pool);
        }
    }
    return m_available_pools.empty() ? nullptr : m_available_pools.back();
}

vk::DescriptorPool PerFrameDescriptorSetAllocator::createPool()
{
    static constexpr uint32_t kSetsPerPool = 64u;
    static const std::vector<vk::DescriptorPoolSize> kPoolSizes = {
        { vk::DescriptorType::eUniformBuffer,        kSetsPerPool * 2u },
        { vk::DescriptorType::eUniformBufferDynamic, kSetsPerPool * 2u },
        { vk::DescriptorType::eStorageBuffer,        kSetsPerPool      },
        { vk::DescriptorType::eCombinedImageSampler, kSetsPerPool * 2u },
        { vk::DescriptorType::eSampledImage,         kSetsPerPool * 2u },
        { vk::DescriptorType::eSampler,              kSetsPerPool      },
    };

    vk::DescriptorPoolCreateInfo pool_info;
    pool_info.setMaxSets(kSetsPerPool)
             .setPoolSizes(kPoolSizes);

    vk::DescriptorPool pool;
    try {
        pool = m_context_p->getDevice().createDescriptorPool(pool_info);
    } catch (const vk::SystemError &) {}
    return pool;
}

// ============================================================================
//  detail::BindlessDescriptorSetAllocator
// ============================================================================

BindlessDescriptorSetAllocator::~BindlessDescriptorSetAllocator()
{
    this->destroy();
}

void BindlessDescriptorSetAllocator::create(VulkanContext * context_p)
{
    m_context_p = context_p;
}

void BindlessDescriptorSetAllocator::destroy()
{
    if (not m_context_p) { return; }
    auto device = m_context_p->getDevice();
    if (m_pool)          { device.destroyDescriptorPool(m_pool); }
    if (m_incoming_pool) { device.destroyDescriptorPool(m_incoming_pool); }
    for (auto & retired : m_retired_pools) { device.destroyDescriptorPool(retired.pool); }
    m_pool             = nullptr;
    m_incoming_pool    = nullptr;
    m_retired_pools.clear();
    m_current_capacity = 0u;
}

VulkanDescriptorSet2 BindlessDescriptorSetAllocator::allocate(
    const VulkanDescriptorSetLayout2 & layout,
    uint32_t initial_variable_count)
{
    m_layout_p = &layout;

    if (not m_pool) {
        m_pool = this->createPool(initial_variable_count, layout);
        if (not m_pool) { return {}; }
        m_descriptor_set   = this->allocateFromPool(m_pool, layout, initial_variable_count);
        m_current_capacity = initial_variable_count;
    }

    return VulkanDescriptorSet2{m_context_p, m_descriptor_set, layout.getBindings(),
                                layout.getStrategy(), layout.getIndex()};
}

void BindlessDescriptorSetAllocator::beginFrame(uint32_t frame_index, uint32_t frames_in_flight)
{
    if (not m_context_p) { return; }
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

void BindlessDescriptorSetAllocator::triggerGrowth(const VulkanDescriptorSetLayout2 & layout)
{
    if (m_growth_state != GrowthState::eIdle) { return; }
    uint32_t new_capacity = m_current_capacity * 2u;
    m_incoming_pool = this->createPool(new_capacity, layout);
    if (not m_incoming_pool) { return; }
    m_incoming_set      = this->allocateFromPool(m_incoming_pool, layout, new_capacity);
    m_incoming_capacity = new_capacity;
    m_growth_state      = GrowthState::eBuilding;
}

vk::DescriptorPool BindlessDescriptorSetAllocator::createPool(
    uint32_t variable_count,
    const VulkanDescriptorSetLayout2 & layout)
{
    std::vector<vk::DescriptorPoolSize> pool_sizes;
    for (const auto & b : layout.getBindings()) {
        bool is_variable = static_cast<bool>(
            b.flags & vk::DescriptorBindingFlagBits::eVariableDescriptorCount);
        uint32_t count = is_variable ? variable_count : b.descriptorCount;
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

vk::DescriptorSet BindlessDescriptorSetAllocator::allocateFromPool(
    vk::DescriptorPool pool,
    const VulkanDescriptorSetLayout2 & layout,
    uint32_t variable_count)
{
    vk::DescriptorSetAllocateInfo alloc_info;
    alloc_info.setDescriptorPool(pool)
              .setSetLayouts(layout.getHandle());

    vk::DescriptorSetVariableDescriptorCountAllocateInfo variable_count_info;
    bool has_variable = std::ranges::any_of(layout.getBindings(), [](const auto & b) {
        return static_cast<bool>(b.flags & vk::DescriptorBindingFlagBits::eVariableDescriptorCount);
    });

    if (has_variable) {
        variable_count_info.setDescriptorCounts(variable_count);
        alloc_info.setPNext(&variable_count_info);
    }

    try {
        return m_context_p->getDevice().allocateDescriptorSets(alloc_info).front();
    } catch (const vk::SystemError &) {}
    return nullptr;
}

// ============================================================================
//  detail::PersistentDescriptorSetAllocator
// ============================================================================

PersistentDescriptorSetAllocator::~PersistentDescriptorSetAllocator()
{
    this->destroy();
}

void PersistentDescriptorSetAllocator::create(VulkanContext * context_p)
{
    m_context_p = context_p;
}

void PersistentDescriptorSetAllocator::destroy()
{
    if (not m_context_p) { return; }
    auto device = m_context_p->getDevice();
    for (auto pool : m_available_pools) { device.destroyDescriptorPool(pool); }
    for (auto pool : m_full_pools)      { device.destroyDescriptorPool(pool); }
    m_available_pools.clear();
    m_full_pools.clear();
}

VulkanDescriptorSet2 PersistentDescriptorSetAllocator::allocate(const VulkanDescriptorSetLayout2 & layout)
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

    return VulkanDescriptorSet2{m_context_p, handle, layout.getBindings(), layout.getStrategy(), layout.getIndex()};
}

void PersistentDescriptorSetAllocator::free(vk::DescriptorSet descriptor_set)
{
    if (not descriptor_set) { return; }
    for (auto pool : m_available_pools) {
        m_context_p->getDevice().freeDescriptorSets(pool, descriptor_set);
        return;
    }
}

vk::DescriptorPool PersistentDescriptorSetAllocator::acquirePool()
{
    if (m_available_pools.empty()) {
        if (auto pool = this->createPool()) {
            m_available_pools.emplace_back(pool);
        }
    }
    return m_available_pools.empty() ? nullptr : m_available_pools.back();
}

vk::DescriptorPool PersistentDescriptorSetAllocator::createPool()
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

// ============================================================================
//  VulkanDescriptorSetAllocator2 — public dispatcher
// ============================================================================

VulkanDescriptorSetAllocator2::~VulkanDescriptorSetAllocator2()
{
    // sub-allocator dtors handle cleanup via their own destroy()
}

void VulkanDescriptorSetAllocator2::create(VulkanContext * context_p)
{
    m_context_p = context_p;
    m_per_frame.create(context_p);
    m_bindless.create(context_p);
    m_persistent.create(context_p);
}

VulkanDescriptorSet2 VulkanDescriptorSetAllocator2::allocate(const VulkanDescriptorSetLayout2 & layout)
{
    switch (layout.getStrategy()) {
        case Strategy::ePerFrame:
            return m_per_frame.allocate(layout);
        case Strategy::eBindless: {
            uint32_t initial_count = vkconstants::bindless::k_initial_variable_descriptor_count;
            return m_bindless.allocate(layout, initial_count);
        }
        case Strategy::ePersistent:
            return m_persistent.allocate(layout);
    }
    return {};
}

void VulkanDescriptorSetAllocator2::deallocate(VulkanDescriptorSet2 & ds) const
{
    switch (ds.getStrategy()) {
        case Strategy::ePerFrame:
            break;  // no-op — batch reset
        case Strategy::eBindless:
            break;  // no-op — pool owns the single set
        case Strategy::ePersistent:
            m_persistent.free(ds.getHandle());
            break;
    }
}

void VulkanDescriptorSetAllocator2::resetPerFrame()
{
    m_per_frame.resetForFrame();
}

void VulkanDescriptorSetAllocator2::beginFrame(uint32_t frame_index, uint32_t frames_in_flight)
{
    m_bindless.beginFrame(frame_index, frames_in_flight);
}
