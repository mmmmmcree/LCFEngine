#include "Vulkan/ds/details/VulkanBindlessDescriptorSet.h"
#include "Vulkan/ds/details/VulkanDescriptorSetAllocator2.h"
#include "Vulkan/ds/VulkanDescriptorSetLayout2.h"
#include "Vulkan/vulkan_constants.h"
#include <utility>

using namespace lcf::render;
using namespace lcf::render::detail;

// ============================================================================
//  detail::VulkanBindlessDescriptorSet
// ============================================================================

VulkanBindlessDescriptorSet::~VulkanBindlessDescriptorSet()
{
    this->destroy();
}

VulkanBindlessDescriptorSet::VulkanBindlessDescriptorSet(Self && other) noexcept :
    m_allocator_p(std::exchange(other.m_allocator_p, nullptr)),
    m_layout_p(std::exchange(other.m_layout_p, nullptr)),
    m_frame_slots(std::move(other.m_frame_slots)),
    m_current_frame(other.m_current_frame),
    m_growth_state(other.m_growth_state),
    m_incoming_slot(std::move(other.m_incoming_slot)),
    m_retired_slots(std::move(other.m_retired_slots))
{
}

VulkanBindlessDescriptorSet & VulkanBindlessDescriptorSet::operator=(Self && other) noexcept
{
    if (this == &other) { return *this; }
    this->destroy();
    m_allocator_p   = std::exchange(other.m_allocator_p, nullptr);
    m_layout_p      = std::exchange(other.m_layout_p, nullptr);
    m_frame_slots   = std::move(other.m_frame_slots);
    m_current_frame = other.m_current_frame;
    m_growth_state  = other.m_growth_state;
    m_incoming_slot = std::move(other.m_incoming_slot);
    m_retired_slots = std::move(other.m_retired_slots);
    return *this;
}

void VulkanBindlessDescriptorSet::create(
    VulkanDescriptorSetAllocator2 & allocator,
    const VulkanDescriptorSetLayout2 & layout,
    uint32_t initial_variable_count,
    uint32_t frame_copies)
{
    m_allocator_p = &allocator;
    m_layout_p    = &layout;

    m_frame_slots.resize(frame_copies);
    for (auto & slot : m_frame_slots) {
        auto result = allocator.allocateBindless(layout, initial_variable_count);
        if (result) {
            slot.set = std::move(*result);
        }
        slot.variable_count = initial_variable_count;
    }
}

void VulkanBindlessDescriptorSet::destroy()
{
    if (not m_allocator_p) { return; }

    for (auto & slot : m_frame_slots) {
        if (slot.set) { m_allocator_p->deallocate(std::move(slot.set)); }
    }
    m_frame_slots.clear();

    if (m_incoming_slot.set) {
        m_allocator_p->deallocate(std::move(m_incoming_slot.set));
        m_incoming_slot = {};
    }

    for (auto & slot : m_retired_slots) {
        if (slot.set) { m_allocator_p->deallocate(std::move(slot.set)); }
    }
    m_retired_slots.clear();

    m_allocator_p  = nullptr;
    m_layout_p     = nullptr;
    m_growth_state = GrowthState::eIdle;
}

void VulkanBindlessDescriptorSet::beginFrame(uint32_t frame_index, uint32_t frames_in_flight)
{
    if (not m_allocator_p || m_frame_slots.empty()) { return; }

    m_current_frame = frame_index % static_cast<uint32_t>(m_frame_slots.size());

    if (m_growth_state == GrowthState::eBuilding) {
        m_growth_state = GrowthState::eSwapPending;
    } else if (m_growth_state == GrowthState::eSwapPending) {
        auto & current = m_frame_slots[m_current_frame];
        // Retire the old slot — it may still be referenced by in-flight frames
        if (current.set) {
            m_retired_slots.push_back(std::move(current));
        }
        current = std::move(m_incoming_slot);
        m_incoming_slot = {};
        m_growth_state  = GrowthState::eIdle;
    }

    // Clean up retired slots that are guaranteed to have drained
    // Conservative: keep for frames_in_flight extra frames
    if (not m_retired_slots.empty() && m_retired_slots.size() > frames_in_flight) {
        auto & oldest = m_retired_slots.front();
        if (oldest.set) { m_allocator_p->deallocate(std::move(oldest.set)); }
        m_retired_slots.erase(m_retired_slots.begin());
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
    if (m_growth_state != GrowthState::eIdle || not m_layout_p || not m_allocator_p) { return; }
    if (m_frame_slots.empty()) { return; }

    uint32_t current_capacity = m_frame_slots[m_current_frame].variable_count;
    uint32_t new_capacity = current_capacity * 2u;

    if (new_capacity > vkconstants::ds::k_max_variable_descriptor_count) {
        new_capacity = vkconstants::ds::k_max_variable_descriptor_count;
    }
    if (new_capacity <= current_capacity) { return; }

    auto result = m_allocator_p->allocateBindless(*m_layout_p, new_capacity);
    if (not result) { return; }

    m_incoming_slot.set            = std::move(*result);
    m_incoming_slot.variable_count = new_capacity;
    m_growth_state = GrowthState::eBuilding;
}
