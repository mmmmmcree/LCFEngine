#include "Vulkan/VulkanDescriptorSet2.h"

using namespace lcf::render;

VulkanDescriptorSetUpdater2 VulkanDescriptorSet2::createUpdater(vk::Device device) const noexcept
{
    return VulkanDescriptorSetUpdater2{device, m_descriptor_set, m_binding_list};
}
