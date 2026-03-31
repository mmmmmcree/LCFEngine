#include "Vulkan/VulkanDescriptorSetManager.h"
#include "Vulkan/VulkanDescriptorSetLayout2.h"
#include "Vulkan/VulkanContext.h"
#include "Vulkan/vulkan_constants.h"
#include <algorithm>
#include <utility>

using namespace lcf::render;
using namespace lcf::render::detail;
using Strategy = vkenums::DescriptorSetStrategy;
using DSIndex  = vkenums::DescriptorSetIndex;

// ============================================================================
//  VulkanDescriptorSetManager — lifecycle
// ============================================================================

// VulkanDescriptorSetManager::~VulkanDescriptorSetManager() = default;

VulkanDescriptorSetManager::VulkanDescriptorSetManager(Self && other) noexcept :
    m_context_p(std::exchange(other.m_context_p, nullptr)),
    m_allocator(std::move(other.m_allocator)),
    m_retired_bindless_sets(std::move(other.m_retired_bindless_sets)),
    m_bindless_buffer_set(std::move(other.m_bindless_buffer_set)),
    m_bindless_texture_set(std::move(other.m_bindless_texture_set))
{
}

VulkanDescriptorSetManager & VulkanDescriptorSetManager::operator=(Self && other) noexcept
{
    if (this == &other) { return *this; }
    m_context_p             = std::exchange(other.m_context_p, nullptr);
    m_allocator             = std::move(other.m_allocator);
    m_retired_bindless_sets = std::move(other.m_retired_bindless_sets);
    m_bindless_buffer_set   = std::move(other.m_bindless_buffer_set);
    m_bindless_texture_set  = std::move(other.m_bindless_texture_set);
    return *this;
}

void VulkanDescriptorSetManager::create(VulkanContext * context_p)
{
    m_context_p = context_p;
    m_allocator.create(context_p);
}

// ============================================================================
//  VulkanDescriptorSetManager — per-frame / persistent
// ============================================================================

VulkanDescriptorSet2 VulkanDescriptorSetManager::createSet(const VulkanDescriptorSetLayout2 & layout)
{
    auto strategy = layout.getStrategy();
    if (strategy == Strategy::eBindless) { return {}; }
    return m_allocator.allocate(layout);
}

void VulkanDescriptorSetManager::destroySet(VulkanDescriptorSet2 & ds)
{
    if (not ds) { return; }
    auto strategy = ds.getStrategy();
    if (strategy == Strategy::eBindless) { return; }
    m_allocator.deallocate(ds.getHandle(), strategy);
}

// ============================================================================
//  VulkanDescriptorSetManager — bindless (internal, friend-accessed)
// ============================================================================

VulkanDescriptorSet2 VulkanDescriptorSetManager::createBindlessSet(
    const VulkanDescriptorSetLayout2 & layout,
    uint32_t variable_count)
{
    return m_allocator.allocateBindless(layout, variable_count);
}

void VulkanDescriptorSetManager::destroyBindlessSet(VulkanDescriptorSet2 & ds)
{
    if (not ds) { return; }
    m_allocator.deallocateBindless(ds.getHandle());
}

void VulkanDescriptorSetManager::retireBindlessSet(VulkanDescriptorSet2 && ds, uint32_t retire_after_frame)
{
    if (not ds) { return; }
    m_retired_bindless_sets.push_back({ std::move(ds), retire_after_frame });
}

// ============================================================================
//  VulkanDescriptorSetManager — bindless initialization and access
// ============================================================================

void VulkanDescriptorSetManager::createBindlessSets(
    const VulkanDescriptorSetLayout2 & buffer_layout,
    const VulkanDescriptorSetLayout2 & texture_layout)
{
    uint32_t initial_count = vkconstants::bindless::k_initial_variable_descriptor_count;

    static constexpr uint32_t k_frames_in_flight = 2u;
    m_bindless_buffer_set.create(this, buffer_layout, initial_count, k_frames_in_flight);
    m_bindless_texture_set.create(this, texture_layout, initial_count, 1u);
}

VulkanDescriptorSet2 & VulkanDescriptorSetManager::getBindlessSet(vkenums::DescriptorSetIndex index) noexcept
{
    if (index == DSIndex::eBindlessTextures) { return m_bindless_texture_set.currentSet(); }
    return m_bindless_buffer_set.currentSet();
}

const VulkanDescriptorSet2 & VulkanDescriptorSetManager::getBindlessSet(vkenums::DescriptorSetIndex index) const noexcept
{
    if (index == DSIndex::eBindlessTextures) { return m_bindless_texture_set.currentSet(); }
    return m_bindless_buffer_set.currentSet();
}

// ============================================================================
//  VulkanDescriptorSetManager — frame
// ============================================================================

void VulkanDescriptorSetManager::beginFrame(uint32_t frame_index, uint32_t frames_in_flight)
{
    m_allocator.resetPools(Strategy::ePerFrame);
    this->cleanupRetiredSets(frame_index);
    m_bindless_buffer_set.beginFrame(frame_index, frames_in_flight);
    m_bindless_texture_set.beginFrame(frame_index, frames_in_flight);
}

void VulkanDescriptorSetManager::cleanupRetiredSets(uint32_t current_frame)
{
    std::erase_if(m_retired_bindless_sets, [&](RetiredEntry & entry) {
        if (current_frame >= entry.retire_frame) {
            m_allocator.deallocateBindless(entry.set.getHandle());
            return true;
        }
        return false;
    });
}

// ============================================================================
//  detail::VulkanBindlessDescriptorSet
// ============================================================================

VulkanBindlessDescriptorSet::~VulkanBindlessDescriptorSet()
{
    this->destroy();
}

VulkanBindlessDescriptorSet::VulkanBindlessDescriptorSet(Self && other) noexcept :
    m_manager_p(std::exchange(other.m_manager_p, nullptr)),
    m_layout_p(std::exchange(other.m_layout_p, nullptr)),
    m_frame_slots(std::move(other.m_frame_slots)),
    m_current_frame(other.m_current_frame),
    m_growth_state(other.m_growth_state),
    m_incoming_slot(std::move(other.m_incoming_slot))
{
}

VulkanBindlessDescriptorSet & VulkanBindlessDescriptorSet::operator=(Self && other) noexcept
{
    if (this == &other) { return *this; }
    this->destroy();
    m_manager_p     = std::exchange(other.m_manager_p, nullptr);
    m_layout_p      = std::exchange(other.m_layout_p, nullptr);
    m_frame_slots   = std::move(other.m_frame_slots);
    m_current_frame = other.m_current_frame;
    m_growth_state  = other.m_growth_state;
    m_incoming_slot = std::move(other.m_incoming_slot);
    return *this;
}

void VulkanBindlessDescriptorSet::create(
    VulkanDescriptorSetManager * manager_p,
    const VulkanDescriptorSetLayout2 & layout,
    uint32_t initial_variable_count,
    uint32_t frame_copies)
{
    m_manager_p = manager_p;
    m_layout_p  = &layout;

    m_frame_slots.resize(frame_copies);
    for (auto & slot : m_frame_slots) {
        slot.set            = m_manager_p->createBindlessSet(layout, initial_variable_count);
        slot.variable_count = initial_variable_count;
    }
}

void VulkanBindlessDescriptorSet::destroy()
{
    if (not m_manager_p) { return; }

    for (auto & slot : m_frame_slots) {
        m_manager_p->destroyBindlessSet(slot.set);
    }
    m_frame_slots.clear();

    if (m_incoming_slot.set) {
        m_manager_p->destroyBindlessSet(m_incoming_slot.set);
        m_incoming_slot = {};
    }

    m_manager_p    = nullptr;
    m_layout_p     = nullptr;
    m_growth_state = GrowthState::eIdle;
}

void VulkanBindlessDescriptorSet::beginFrame(uint32_t frame_index, uint32_t frames_in_flight)
{
    if (not m_manager_p || m_frame_slots.empty()) { return; }

    m_current_frame = frame_index % static_cast<uint32_t>(m_frame_slots.size());

    if (m_growth_state == GrowthState::eBuilding) {
        m_growth_state = GrowthState::eSwapPending;
    } else if (m_growth_state == GrowthState::eSwapPending) {
        auto & current = m_frame_slots[m_current_frame];
        m_manager_p->retireBindlessSet(std::move(current.set), frame_index + frames_in_flight);
        current = std::move(m_incoming_slot);
        m_incoming_slot = {};
        m_growth_state  = GrowthState::eIdle;
    }
}

VulkanDescriptorSet2 & VulkanBindlessDescriptorSet::currentSet() noexcept
{
    return m_frame_slots[m_current_frame].set;
}

const VulkanDescriptorSet2 & VulkanBindlessDescriptorSet::currentSet() const noexcept
{
    return m_frame_slots[m_current_frame].set;
}

void VulkanBindlessDescriptorSet::requestGrowth()
{
    if (m_growth_state != GrowthState::eIdle || not m_layout_p || not m_manager_p) { return; }
    if (m_frame_slots.empty()) { return; }

    uint32_t current_capacity = m_frame_slots[m_current_frame].variable_count;
    uint32_t new_capacity = current_capacity * 2u;

    if (new_capacity > vkconstants::bindless::k_max_variable_descriptor_count) {
        new_capacity = vkconstants::bindless::k_max_variable_descriptor_count;
    }
    if (new_capacity <= current_capacity) { return; }

    m_incoming_slot.set            = m_manager_p->createBindlessSet(*m_layout_p, new_capacity);
    m_incoming_slot.variable_count = new_capacity;

    if (not m_incoming_slot.set) {
        m_incoming_slot = {};
        return;
    }
    m_growth_state = GrowthState::eBuilding;
}