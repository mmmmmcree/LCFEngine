#include "vulkan/VulkanDescriptorSetUpdater.h"
#include "vulkan/VulkanContext.h"

using namespace lcf::render;

lcf::render::VulkanDescriptorSetUpdater::VulkanDescriptorSetUpdater(
    VulkanContext * context_p,
    vk::DescriptorSet target_set,
    BindingReadSpan binding_span) :
    m_context_p(context_p),
    m_target_set(target_set),
    m_binding_span(binding_span)
{
}

VulkanDescriptorSetUpdater & VulkanDescriptorSetUpdater::add(uint32_t binding, const vk::DescriptorImageInfo &image_info)
{
    return this->add(binding, m_index_offset, image_info);
}

VulkanDescriptorSetUpdater & VulkanDescriptorSetUpdater::add(uint32_t binding, uint32_t index, const vk::DescriptorImageInfo &image_info)
{
    const auto &binding_info = m_binding_span[binding];
    vk::WriteDescriptorSet & write_descriptor_set = m_write_descriptor_sets.emplace_back();
    write_descriptor_set.setDstBinding(binding_info.binding)
        .setDstArrayElement(index + m_index_offset)
        .setDescriptorType(binding_info.descriptorType)
        .setDescriptorCount(binding_info.descriptorCount)
        .setImageInfo(image_info);
    return *this;    
}

VulkanDescriptorSetUpdater & VulkanDescriptorSetUpdater::add(uint32_t binding, const vk::DescriptorBufferInfo &buffer_info)
{
    return this->add(binding, m_index_offset, buffer_info);
}

VulkanDescriptorSetUpdater & lcf::render::VulkanDescriptorSetUpdater::add(uint32_t binding, uint32_t index, const vk::DescriptorBufferInfo &buffer_info)
{
    const auto &binding_info = m_binding_span[binding];
    vk::WriteDescriptorSet & write_descriptor_set = m_write_descriptor_sets.emplace_back();
    write_descriptor_set.setDstBinding(binding_info.binding)
        .setDstArrayElement(index + m_index_offset)
        .setDescriptorType(binding_info.descriptorType)
        .setDescriptorCount(binding_info.descriptorCount)
        .setBufferInfo(buffer_info);
    return *this;    
}

void VulkanDescriptorSetUpdater::update()
{
    auto device = m_context_p->getDevice();
    for (auto &write_descriptor_set : m_write_descriptor_sets) {
        write_descriptor_set.setDstSet(m_target_set);
    }
    device.updateDescriptorSets(m_write_descriptor_sets, nullptr);
}