#include "VulkanDescriptorSetPrototype.h"
#include "VulkanContext.h"

void lcf::render::VulkanDescriptorSetPrototype::create(VulkanContext * context_p)
{
    m_context_p = context_p;
    auto device = m_context_p->getDevice();
    vk::DescriptorSetLayoutCreateInfo layout_info;
    layout_info.setBindings(m_bindings);
    m_layout = device.createDescriptorSetLayoutUnique(layout_info);
}