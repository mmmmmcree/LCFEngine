#include "Vulkan/VulkanDescriptorSetLayout.h"
#include "Vulkan/VulkanContext.h"
#include "Vulkan/vulkan_constants.h"

using namespace lcf::render;

void lcf::render::VulkanDescriptorSetLayout::create(VulkanContext *context_p)
{
    m_context_p = context_p;
    auto device = m_context_p->getDevice();
    vk::DescriptorSetLayoutCreateInfo layout_info;
    vk::DescriptorSetLayoutBindingFlagsCreateInfo binding_flags_info;
    if (not m_binding_list.empty() and m_binding_list.back().descriptorCount == 0) {
        m_binding_list.back().descriptorCount = vkconstants::ds::k_max_variable_descriptor_count;
        m_binding_flags_list.resize(m_binding_list.size());
        m_binding_flags_list.back() = vk::FlagTraits<vk::DescriptorBindingFlagBits>::allFlags;
        binding_flags_info.setBindingFlags(m_binding_flags_list);
        m_flags |= vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool;
    }
    layout_info.setBindings(m_binding_list)
        .setFlags(m_flags)
        .setPNext(&binding_flags_info);
    m_layout = device.createDescriptorSetLayoutUnique(layout_info);
}
