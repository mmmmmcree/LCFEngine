#include "vulkan/VulkanDescriptorSetLayout.h"
#include "vulkan/VulkanContext.h"

using namespace lcf::render;

void lcf::render::VulkanDescriptorSetLayout::create(VulkanContext *context_p)
{
    static constexpr uint64_t DESCRIPTOR_COUNT_THRESHOLD = 256;
    m_context_p = context_p;
    auto device = m_context_p->getDevice();
    vk::DescriptorSetLayoutCreateInfo layout_info;
    vk::DescriptorSetLayoutBindingFlagsCreateInfo binding_flags_info;
    if (not m_binding_list.empty() and m_binding_list.back().descriptorCount > DESCRIPTOR_COUNT_THRESHOLD) {
        m_binding_flags_list.resize(m_binding_list.size());
        m_binding_flags_list.back() = vk::FlagTraits<vk::DescriptorBindingFlagBits>::allFlags;
        binding_flags_info.setBindingFlags(m_binding_flags_list);
        layout_info.setPNext(&binding_flags_info);
        m_flags |= vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool;
    }
    layout_info.setBindings(m_binding_list)
        .setFlags(m_flags);
    m_layout = device.createDescriptorSetLayoutUnique(layout_info);
}
