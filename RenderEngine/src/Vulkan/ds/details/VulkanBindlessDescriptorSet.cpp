#include "Vulkan/ds/details/VulkanBindlessDescriptorSet.h"
#include "Vulkan/ds/VulkanDescriptorSetManager.h"
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
        // slot.set            = m_manager_p->createBindlessSet(layout, initial_variable_count);
        slot.variable_count = initial_variable_count;
    }
}

void VulkanBindlessDescriptorSet::destroy()
{
    if (not m_manager_p) { return; }

    for (auto & slot : m_frame_slots) {
        // m_manager_p->destroyBindlessSet(slot.set);
    }
    m_frame_slots.clear();

    if (m_incoming_slot.set) {
        // m_manager_p->destroyBindlessSet(m_incoming_slot.set);
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
        // m_manager_p->retireBindlessSet(std::move(current.set), frame_index + frames_in_flight);
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

    if (new_capacity > vkconstants::ds::k_max_variable_descriptor_count) {
        new_capacity = vkconstants::ds::k_max_variable_descriptor_count;
    }
    if (new_capacity <= current_capacity) { return; }

    // m_incoming_slot.set            = m_manager_p->createBindlessSet(*m_layout_p, new_capacity);
    m_incoming_slot.variable_count = new_capacity;

    if (not m_incoming_slot.set) {
        m_incoming_slot = {};
        return;
    }
    m_growth_state = GrowthState::eBuilding;
}
