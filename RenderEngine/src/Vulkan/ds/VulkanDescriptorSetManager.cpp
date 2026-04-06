#include "Vulkan/ds/VulkanDescriptorSetManager.h"
#include "Vulkan/VulkanContext.h"
#include "Vulkan/vulkan_constants.h"
#include "enums/enum_count.h"

using namespace lcf::render;
namespace stdv = std::views;

// ============================================================================
//  VulkanDescriptorSetManager — lifecycle
// ============================================================================

void VulkanDescriptorSetManager::create(VulkanContext * context_p)
{
    m_context_p = context_p;
    vk::Device device = context_p->getDevice();
    m_allocator.create(device);
    createBindlessLayouts(device);
    createBindlessSets();
}

void VulkanDescriptorSetManager::createBindlessLayouts(vk::Device device)
{
    m_bindless_buffer_layout
        .setBindings(vkconstants::ds::k_bindless_buffer_bindings)
        .setIndex(vkenums::decode::get_index(vkenums::DescriptorSetIndex::eBindlessBuffers));
    m_bindless_buffer_layout.create(device, vkenums::DescriptorSetStrategy::eBindless);

    m_bindless_texture_layout
        .setBindings(vkconstants::ds::k_bindless_texture_bindings)
        .setIndex(vkenums::decode::get_index(vkenums::DescriptorSetIndex::eBindlessTextures));
    m_bindless_texture_layout.create(device, vkenums::DescriptorSetStrategy::eBindless);
}

void VulkanDescriptorSetManager::createBindlessSets()
{
    static constexpr uint32_t k_buffer_frame_copies  = 2u;
    // static constexpr uint32_t k_texture_frame_copies = 2u;
    // const uint32_t buffer_binding_count = lcf::enum_count_v<vkenums::internal::BindlessBufferBinding>;
    // const uint32_t texture_binding_count = lcf::enum_count_v<vkenums::internal::BindlessTextureBinding>;
    // m_bindless_buffer_set.create(m_allocator, m_bindless_buffer_layout, buffer_binding_count, initial_count, k_buffer_frame_copies);
    // m_bindless_texture_set.create(m_allocator, m_bindless_texture_layout, texture_binding_count, initial_count, k_texture_frame_copies);
}

// ============================================================================
//  VulkanDescriptorSetManager — individual DS
// ============================================================================

VulkanDescriptorSet2 VulkanDescriptorSetManager::allocateSet(const VulkanDescriptorSetLayout2 & layout)
{
    auto result = m_allocator.allocate(layout);
    if (not result) { return {}; }
    return std::move(*result);
}

void VulkanDescriptorSetManager::deallocateSet(VulkanDescriptorSet2 && ds)
{
    if (not ds) { return; }
    m_allocator.deallocate(std::move(ds));
}

// ============================================================================
//  VulkanDescriptorSetManager — bindless access
// ============================================================================

VulkanDescriptorSet2 & VulkanDescriptorSetManager::getBindlessBufferSet() noexcept
{
    return m_bindless_buffer_set.getSet();
}

const VulkanDescriptorSet2 & VulkanDescriptorSetManager::getBindlessBufferSet() const noexcept
{
    return m_bindless_buffer_set.getSet();
}

VulkanDescriptorSet2 & VulkanDescriptorSetManager::getBindlessTextureSet() noexcept
{
    return m_bindless_texture_set.getSet();
}

const VulkanDescriptorSet2 & VulkanDescriptorSetManager::getBindlessTextureSet() const noexcept
{
    return m_bindless_texture_set.getSet();
}

// ============================================================================
//  VulkanDescriptorSetManager — bindless writes
// ============================================================================

void VulkanDescriptorSetManager::addBindlessBufferInfo(
    uint32_t binding, uint32_t array_index, const vk::DescriptorBufferInfo & info)
{
    m_bindless_buffer_set.addDescriptorInfo(binding, array_index, info);
}

void VulkanDescriptorSetManager::addBindlessTextureInfo(
    uint32_t binding, uint32_t array_index, const vk::DescriptorImageInfo & info)
{
    m_bindless_texture_set.addDescriptorInfo(binding, array_index, info);
}

// ============================================================================
//  VulkanDescriptorSetManager — frame
// ============================================================================

void VulkanDescriptorSetManager::beginFrame(uint32_t frame_index, uint32_t frames_in_flight)
{
    vk::Device device = m_context_p->getDevice();
    m_bindless_buffer_set.beginFrame(frame_index, frames_in_flight, device);
    m_bindless_texture_set.beginFrame(frame_index, frames_in_flight, device);
}
