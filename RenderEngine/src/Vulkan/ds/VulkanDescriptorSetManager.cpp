#include "Vulkan/ds/VulkanDescriptorSetManager.h"
#include "Vulkan/ds/VulkanDescriptorSetLayout2.h"
#include "Vulkan/VulkanContext.h"
#include "Vulkan/vulkan_constants.h"

using namespace lcf::render;

// ============================================================================
//  VulkanDescriptorSetManager — lifecycle
// ============================================================================

void VulkanDescriptorSetManager::create(VulkanContext * context_p)
{
    m_context_p = context_p;
    m_allocator.create(context_p->getDevice());
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
//  VulkanDescriptorSetManager — bindless initialization and access
// ============================================================================

void VulkanDescriptorSetManager::createBindlessSets(
    const VulkanDescriptorSetLayout2 & buffer_layout,
    const VulkanDescriptorSetLayout2 & texture_layout)
{
    uint32_t initial_count = vkconstants::ds::k_initial_variable_descriptor_count;

    static constexpr uint32_t k_buffer_frame_copies  = 2u;  // ring buffer for in-flight frames
    static constexpr uint32_t k_texture_frame_copies = 1u;  // single copy — images don't resize

    m_bindless_buffer_set.create(m_allocator, buffer_layout, initial_count, k_buffer_frame_copies);
    m_bindless_texture_set.create(m_allocator, texture_layout, initial_count, k_texture_frame_copies);
}

VulkanDescriptorSet2 & VulkanDescriptorSetManager::getBindlessBufferSet() noexcept
{
    return m_bindless_buffer_set.currentSet();
}

const VulkanDescriptorSet2 & VulkanDescriptorSetManager::getBindlessBufferSet() const noexcept
{
    return m_bindless_buffer_set.currentSet();
}

VulkanDescriptorSet2 & VulkanDescriptorSetManager::getBindlessTextureSet() noexcept
{
    return m_bindless_texture_set.currentSet();
}

const VulkanDescriptorSet2 & VulkanDescriptorSetManager::getBindlessTextureSet() const noexcept
{
    return m_bindless_texture_set.currentSet();
}

// ============================================================================
//  VulkanDescriptorSetManager — frame
// ============================================================================

void VulkanDescriptorSetManager::beginFrame(uint32_t frame_index, uint32_t frames_in_flight)
{
    m_bindless_buffer_set.beginFrame(frame_index, frames_in_flight);
    m_bindless_texture_set.beginFrame(frame_index, frames_in_flight);
}
