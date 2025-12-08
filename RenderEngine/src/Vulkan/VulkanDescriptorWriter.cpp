#include "Vulkan/VulkanDescriptorWriter.h"
#include "Vulkan/VulkanContext.h"

using namespace lcf::render;

VulkanDescriptorWriter::VulkanDescriptorWriter(VulkanContext *context_p, DescriptorSetLayoutBindings binding_list) :
    m_context_p(context_p),
    m_layout_binding_list(binding_list)
{
}

VulkanDescriptorWriter & VulkanDescriptorWriter::add(uint32_t binding, const vk::DescriptorImageInfo &image_info)
{
    return this->add(binding, 0, image_info);
}

VulkanDescriptorWriter & VulkanDescriptorWriter::add(uint32_t binding, uint32_t index, const vk::DescriptorImageInfo &image_info)
{
    const auto &binding_info = m_layout_binding_list[binding];
    vk::WriteDescriptorSet & write_descriptor_set = m_write_descriptor_sets.emplace_back();
    write_descriptor_set.setDstBinding(binding_info.binding)
        .setDstArrayElement(index)
        .setDescriptorType(binding_info.descriptorType)
        .setDescriptorCount(binding_info.descriptorCount)
        .setImageInfo(image_info);
    return *this;    
}

VulkanDescriptorWriter & VulkanDescriptorWriter::add(uint32_t binding, const vk::DescriptorBufferInfo &buffer_info)
{
    return this->add(binding, 0, buffer_info);
}

VulkanDescriptorWriter & lcf::render::VulkanDescriptorWriter::add(uint32_t binding, uint32_t index, const vk::DescriptorBufferInfo &buffer_info)
{
    const auto &binding_info = m_layout_binding_list[binding];
    vk::WriteDescriptorSet & write_descriptor_set = m_write_descriptor_sets.emplace_back();
    write_descriptor_set.setDstBinding(binding_info.binding)
        .setDstArrayElement(index)
        .setDescriptorType(binding_info.descriptorType)
        .setDescriptorCount(binding_info.descriptorCount)
        .setBufferInfo(buffer_info);
    return *this;    
}

void lcf::render::VulkanDescriptorWriter::write(vk::DescriptorSet set)
{
    auto device = m_context_p->getDevice();
    for (auto &write_descriptor_set : m_write_descriptor_sets) {
        write_descriptor_set.setDstSet(set);
    }
    device.updateDescriptorSets(m_write_descriptor_sets, nullptr); //todo: replace nullptr with descriptorCopies
}
