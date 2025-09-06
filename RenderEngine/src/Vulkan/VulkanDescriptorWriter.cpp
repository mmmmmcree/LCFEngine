#include "VulkanDescriptorWriter.h"

lcf::render::VulkanDescriptorWriter::VulkanDescriptorWriter(VulkanContext *context, const DescriptorSetLayoutBindingTable &binding_table, const DescriptorSetList &descriptor_sets) :
    m_context_p(context),
    m_binding_table(binding_table),
    m_descriptor_sets(descriptor_sets)
{
}

lcf::render::VulkanDescriptorWriter &lcf::render::VulkanDescriptorWriter::add(uint32_t set, uint32_t binding, const vk::DescriptorImageInfo &image_info)
{
    return this->add(set, binding, 0, image_info);
}

lcf::render::VulkanDescriptorWriter &lcf::render::VulkanDescriptorWriter::add(uint32_t set, uint32_t binding, uint32_t index, const vk::DescriptorImageInfo &image_info)
{
    const auto &binding_info = m_binding_table[set][binding];
    vk::WriteDescriptorSet & write_descriptor_set = m_write_descriptor_sets.emplace_back();
    write_descriptor_set.setDstSet(m_descriptor_sets[set])
        .setDstBinding(binding_info.binding)
        .setDstArrayElement(index)
        .setDescriptorType(binding_info.descriptorType)
        .setDescriptorCount(binding_info.descriptorCount)
        .setImageInfo(image_info);
    return *this;    
}

lcf::render::VulkanDescriptorWriter &lcf::render::VulkanDescriptorWriter::add(uint32_t set, uint32_t binding, const vk::DescriptorBufferInfo &buffer_info)
{
    return this->add(set, binding, 0, buffer_info);
}

lcf::render::VulkanDescriptorWriter &lcf::render::VulkanDescriptorWriter::add(uint32_t set, uint32_t binding, uint32_t index, const vk::DescriptorBufferInfo &buffer_info)
{
    const auto &binding_info = m_binding_table[set][binding];
    vk::WriteDescriptorSet & write_descriptor_set = m_write_descriptor_sets.emplace_back();
    write_descriptor_set.setDstSet(m_descriptor_sets[set])
        .setDstBinding(binding_info.binding)
        .setDstArrayElement(index)
        .setDescriptorType(binding_info.descriptorType)
        .setDescriptorCount(binding_info.descriptorCount)
        .setBufferInfo(buffer_info);
    return *this;    
}

void lcf::render::VulkanDescriptorWriter::write()
{
    auto device = m_context_p->getDevice();
    device.updateDescriptorSets(m_write_descriptor_sets, nullptr); //todo: replace nullptr with descriptorCopies
}
