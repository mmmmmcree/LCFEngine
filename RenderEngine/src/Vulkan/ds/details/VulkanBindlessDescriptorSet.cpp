#include "Vulkan/ds/details/VulkanBindlessDescriptorSet.h"
#include "Vulkan/ds/details/VulkanDescriptorSetAllocator2.h"
#include "Vulkan/ds/VulkanDescriptorSetLayout2.h"
#include "Vulkan/vulkan_constants.h"
#include <utility>

using namespace lcf::render;
using namespace lcf::render::detail;

// ============================================================================
//  Lifecycle
// ============================================================================

VulkanBindlessDescriptorSet::~VulkanBindlessDescriptorSet()
{
    this->destroy();
}

VulkanBindlessDescriptorSet::VulkanBindlessDescriptorSet(Self && other) noexcept :
    m_allocator_p(std::exchange(other.m_allocator_p, nullptr)),
    m_layout_p(std::exchange(other.m_layout_p, nullptr)),
    m_frame_slots(std::move(other.m_frame_slots)),
    m_retired_slots(std::move(other.m_retired_slots)),
    m_current_frame(other.m_current_frame),
    m_authority_binding_map(std::move(other.m_authority_binding_map))
{
}

VulkanBindlessDescriptorSet & VulkanBindlessDescriptorSet::operator=(Self && other) noexcept
{
    if (this == &other) { return *this; }
    this->destroy();
    m_allocator_p = std::exchange(other.m_allocator_p, nullptr);
    m_layout_p = std::exchange(other.m_layout_p, nullptr);
    m_frame_slots = std::move(other.m_frame_slots);
    m_retired_slots = std::move(other.m_retired_slots);
    m_current_frame = other.m_current_frame;
    m_authority_binding_map = std::move(other.m_authority_binding_map);
    return *this;
}

void VulkanBindlessDescriptorSet::create(
    VulkanDescriptorSetAllocator2 & allocator,
    const VulkanDescriptorSetLayout2 & layout,
    uint32_t binding_count,
    uint32_t initial_variable_count,
    uint32_t frame_copies) noexcept
{
    m_allocator_p = &allocator;
    m_layout_p = &layout;
    m_authority_binding_map.resize(binding_count);
    m_frame_slots.resize(frame_copies);
    for (auto & slot : m_frame_slots) {
        auto result = allocator.allocateBindless(layout, initial_variable_count);
        if (result) { slot.set = std::move(*result); }
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
    for (auto & slot : m_retired_slots) {
        if (slot.set) { m_allocator_p->deallocate(std::move(slot.set)); }
    }
    m_retired_slots.clear();
    m_allocator_p = nullptr;
    m_layout_p = nullptr;
    m_authority_binding_map.clear();
}

// ============================================================================
//  Authority writes
// ============================================================================

VulkanBindlessDescriptorSet & VulkanBindlessDescriptorSet::addDescriptorInfo(
    uint32_t binding, uint32_t array_index, const DescriptorInfo & info)
{
    m_authority_binding_map[binding][array_index] = info;
    std::visit([&](const auto & i) {
        for (auto & slot : m_frame_slots) {
            slot.set.addDescriptorInfo(binding, array_index, i);
        }
    }, info);
    return *this;
}

// ============================================================================
//  Frame advance
// ============================================================================

void VulkanBindlessDescriptorSet::beginFrame(uint32_t frame_index, uint32_t frames_in_flight, vk::Device device)
{
    if (not m_allocator_p || m_frame_slots.empty()) { return; }

    m_current_frame = frame_index % static_cast<uint32_t>(m_frame_slots.size());
    auto & slot = m_frame_slots[m_current_frame];

    const uint32_t required = this->computeRequiredCount();

    if (required > slot.variable_count) {
        // Growth: reallocate all frame slots with doubled capacity
        uint32_t new_capacity = slot.variable_count;
        while (new_capacity < required) { new_capacity *= 2u; }
        if (new_capacity > vkconstants::ds::k_max_variable_descriptor_count) {
            new_capacity = vkconstants::ds::k_max_variable_descriptor_count;
        }
        FrameSlots new_slots(m_frame_slots.size());
        bool all_ok = true;
        for (auto & new_slot : new_slots) {
            new_slot = this->allocateSlot(new_capacity);
            if (not new_slot.set) { all_ok = false; break; }
        }
        if (all_ok) {
            for (auto & old_slot : m_frame_slots) {
                m_retired_slots.emplace_back(std::move(old_slot));
            }
            m_frame_slots = std::move(new_slots);
            // Repopulate all new slots from authority, clear their pending lists
            for (auto & new_slot : m_frame_slots) {
                this->writeAll(new_slot.set, device);
            }
        }
        // else allocation failed — keep old slots, retry next frame
    } else {
        slot.set.commitUpdate(device);
    }

    // Reclaim retired slots that are no longer in-flight
    while (m_retired_slots.size() > frames_in_flight) {
        auto & oldest = m_retired_slots.front();
        if (oldest.set) { m_allocator_p->deallocate(std::move(oldest.set)); }
        m_retired_slots.pop_front();
    }
}

VulkanDescriptorSet2 & VulkanBindlessDescriptorSet::getSet() noexcept
{
    return m_frame_slots[m_current_frame].set;
}

const VulkanDescriptorSet2 & VulkanBindlessDescriptorSet::getSet() const noexcept
{
    return m_frame_slots[m_current_frame].set;
}

// ============================================================================
//  Private helpers
// ============================================================================

uint32_t VulkanBindlessDescriptorSet::computeRequiredCount() const noexcept
{
    if (m_authority_binding_map.empty() || not m_layout_p) { return 0u; }
    const auto & bindings = m_layout_p->getBindings();
    if (bindings.empty()) { return 0u; }
    const uint32_t variable_binding_index = bindings.back().getBindingIndex();
    if (variable_binding_index >= m_authority_binding_map.size()) { return 0u; }
    const auto & auth_slot = m_authority_binding_map[variable_binding_index];
    if (auth_slot.empty()) { return 0u; }
    uint32_t max_index = 0u;
    for (const auto & [array_index, _] : auth_slot) {
        if (array_index > max_index) { max_index = array_index; }
    }
    return max_index + 1u;
}

VulkanBindlessDescriptorSet::Slot VulkanBindlessDescriptorSet::allocateSlot(uint32_t variable_count)
{
    Slot slot;
    auto result = m_allocator_p->allocateBindless(*m_layout_p, variable_count);
    if (result) {
        slot.set = std::move(*result);
        slot.variable_count = variable_count;
    }
    return slot;
}

void VulkanBindlessDescriptorSet::writeAll(VulkanDescriptorSet2 & set, vk::Device device)
{
    for (uint32_t binding = 0u; binding < m_authority_binding_map.size(); ++binding) {
        for (const auto & [array_index, info] : m_authority_binding_map[binding]) {
            std::visit([&](const auto & i) { set.addDescriptorInfo(binding, array_index, i); }, info);
        }
    }
    set.commitUpdate(device);
}
