#include "Vulkan/VulkanDescriptorSetAllocator2.h"
#include "Vulkan/VulkanDescriptorSetLayout2.h"
#include "Vulkan/VulkanContext.h"
#include "Vulkan/vulkan_constants.h"

using namespace lcf::render;
using Strategy = vkenums::DescriptorSetStrategy;

void VulkanDescriptorSetAllocator2::create(VulkanContext * context_p)
{
    m_per_frame_manager.create(context_p);
    m_bindless_manager.create(context_p);
    m_persistent_manager.create(context_p);
}

VulkanDescriptorSet2 VulkanDescriptorSetAllocator2::allocate(const VulkanDescriptorSetLayout2 & layout)
{
    switch (layout.getStrategy()) {
        case Strategy::ePerFrame:
            return m_per_frame_manager.allocate(layout);
        case Strategy::eBindless: {
            uint32_t initial_count = vkconstants::bindless::k_initial_variable_descriptor_count;
            return m_bindless_manager.allocate(layout, initial_count);
        }
        case Strategy::ePersistent:
            return m_persistent_manager.allocate(layout);
    }
    return {};
}

void VulkanDescriptorSetAllocator2::resetPerFrame()
{
    m_per_frame_manager.resetForFrame();
}

void VulkanDescriptorSetAllocator2::beginFrame(uint32_t frame_index, uint32_t frames_in_flight)
{
    m_bindless_manager.beginFrame(frame_index, frames_in_flight);
}
