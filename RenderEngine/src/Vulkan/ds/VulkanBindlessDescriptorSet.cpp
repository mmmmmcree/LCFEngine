#include "Vulkan/ds/VulkanBindlessDescriptorSet.h"
#include "Vulkan/ds/details/VulkanBindlessDescriptorSetAllocator.h"
#include "Vulkan/ds/VulkanDescriptorSetLayout2.h"
#include "Vulkan/vulkan_constants.h"
#include <utility>
#include <ranges>

using namespace lcf::render;
using namespace lcf::render::detail;

VulkanBindlessDescriptorSet::~VulkanBindlessDescriptorSet() noexcept = default;

VulkanBindlessDescriptorSet::operator bool() const noexcept
{
    return std::ranges::all_of(m_frame_slots, [](const auto & slot) { return bool(slot.m_set); });
}

std::error_code VulkanBindlessDescriptorSet::create(
    vk::Device device,
    vkenums::BindlessSetType bindless_set_type,
    uint32_t frame_copies) noexcept
{
    m_allocator_up = std::make_unique<VulkanBindlessDescriptorSetAllocator>();
    if (auto ec = m_allocator_up->create(device)) { return ec; }
    switch (bindless_set_type) {
        case vkenums::BindlessSetType::eBuffer: {
            m_layout.setBindings(vkconstants::ds::k_bindless_buffer_bindings);
            m_layout.setIndex(vkenums::decode::get_index(vkenums::DescriptorSetIndex::eBindlessBuffers));
        } break;
        case vkenums::BindlessSetType::eTexture: {
            m_layout.setBindings(vkconstants::ds::k_bindless_texture_bindings);
            m_layout.setIndex(vkenums::decode::get_index(vkenums::DescriptorSetIndex::eBindlessTextures));
        } break;
    }
    if (auto ec = m_layout.create(device, vkenums::DescriptorSetStrategy::eBindless)) { return ec; }
    m_authority_binding_map.resize(m_layout.getBindings().size());
    m_frame_slots.resize(frame_copies);
    for (auto & slot : m_frame_slots) {
        if (auto ec = this->recreateSlot(device, slot)) { return ec; }
    }
    return {};
}

VulkanBindlessDescriptorSet & VulkanBindlessDescriptorSet::addDescriptorInfo(
    uint32_t binding, uint32_t array_index, const DescriptorInfo & info)
{
    if (binding >= this->getBindings().size()) { return *this; }
    const auto & layout_binding = this->getBindings()[binding];
    if (not layout_binding.containsFlags(vk::DescriptorBindingFlagBits::eVariableDescriptorCount) and
        array_index >= layout_binding.getDescriptorCount()) {
        return *this;
    }
    m_authority_binding_map[binding][array_index].descriptor_info = info;
    for (auto & slot : m_frame_slots) {
        slot.addDescriptorInfo(binding, array_index, info);
    }
    return *this;
}

VulkanBindlessDescriptorSet & VulkanBindlessDescriptorSet::addDescriptorInfo(
    uint32_t binding, uint32_t array_index, const DescriptorInfo & info, ResourceLease lease)
{
    if (binding >= this->getBindings().size()) { return *this; }
    this->addDescriptorInfo(binding, array_index, info);
    m_authority_binding_map[binding][array_index].lease = std::move(lease);
    return *this;
}

void VulkanBindlessDescriptorSet::commitUpdate(vk::Device device) noexcept
{
    m_current_index = (m_current_index + 1u) % static_cast<uint32_t>(m_frame_slots.size());
    auto & slot = m_frame_slots[m_current_index];
    const auto & bindings = m_layout.getBindings();
    if (bindings.containsFlags(vk::DescriptorBindingFlagBits::eVariableDescriptorCount) and
        m_authority_binding_map.back().size() > slot.m_variable_count) {
        this->recreateSlot(device, slot);
    }
    slot.commitUpdate(device);
    while (m_retired_slots.size() > m_frame_slots.size()) {
        auto & oldest = m_retired_slots.front();
        if (oldest.m_set) { m_allocator_up->deallocate(std::move(oldest.m_set)); }
        m_retired_slots.pop_front();
    }
}

const vk::DescriptorSet & VulkanBindlessDescriptorSet::getHandle() const noexcept
{
    return m_frame_slots[m_current_index].m_set.getHandle();
}

std::error_code lcf::render::VulkanBindlessDescriptorSet::recreateSlot(vk::Device device, Slot & slot)
{
    uint32_t variable_count = slot.m_variable_count << 1;
    auto result = m_allocator_up->allocate(m_layout, variable_count);
    if (not result) { return result.error(); }
    m_retired_slots.emplace_back(std::move(slot));
    slot = Slot { std::move(*result), variable_count };
    for (uint32_t binding = 0u; binding < m_authority_binding_map.size(); ++binding) {
        for (const auto & [array_index, auth_binding] : m_authority_binding_map[binding]) {
            const auto & [descriptor_info, lease] = auth_binding;
            slot.addDescriptorInfo(binding, array_index, auth_binding.descriptor_info);
            slot.addLease(binding, array_index, lease);
        }
    }
    return {};
}

VulkanBindlessDescriptorSet::Slot::Slot(VulkanDescriptorSet2 set, uint32_t variable_count) :
    m_set(std::move(set)),
    m_variable_count(variable_count)
{
}

void VulkanBindlessDescriptorSet::Slot::addDescriptorInfo(uint32_t binding, uint32_t array_index, const DescriptorInfo &info)
{
    m_set.addDescriptorInfo(binding, array_index, info);
}

void VulkanBindlessDescriptorSet::Slot::addLease(uint32_t binding, uint32_t array_index, ResourceLease lease)
{
    uint64_t key = (static_cast<uint64_t>(binding) << 32) | static_cast<uint64_t>(array_index);
    m_pending_resource_leases[key] = std::move(lease);
}

void VulkanBindlessDescriptorSet::Slot::commitUpdate(vk::Device device)
{
    m_set.commitUpdate(device);
    m_in_use_resource_leases.insert_range(std::move(m_pending_resource_leases));
    m_pending_resource_leases.clear();
}
