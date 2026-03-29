#include "vulkan/VulkanDescriptorSetUpdater.h"

using namespace lcf::render;

VulkanDescriptorSetUpdater::VulkanDescriptorSetUpdater(
    vk::Device device,
    vk::DescriptorSet target_set,
    BindingReadSpan binding_span) :
    m_device(device),
    m_target_set(target_set),
    m_binding_span(binding_span)
{
}

VulkanDescriptorSetUpdater & VulkanDescriptorSetUpdater::add(uint32_t binding, const vk::DescriptorImageInfo &image_info)
{
    m_image_info_list.emplace_back(image_info);
    return this->add(binding, 0, std::span<const vk::DescriptorImageInfo>(&image_info, 1));
}

VulkanDescriptorSetUpdater & VulkanDescriptorSetUpdater::add(uint32_t binding, uint32_t index, const vk::DescriptorImageInfo &image_info)
{
    m_image_info_list.emplace_back(image_info);
    return this->add(binding, index, std::span<const vk::DescriptorImageInfo>(&image_info, 1));
}

VulkanDescriptorSetUpdater &VulkanDescriptorSetUpdater::add(uint32_t binding, std::span<const vk::DescriptorImageInfo> image_infos)
{
    return this->add(binding, 0, image_infos);
}

VulkanDescriptorSetUpdater & VulkanDescriptorSetUpdater::add(uint32_t binding, uint32_t index, std::span<const vk::DescriptorImageInfo> image_infos)
{
    const auto &binding_info = m_binding_span[binding];
    vk::WriteDescriptorSet & write_descriptor_set = m_write_descriptor_sets.emplace_back();
    write_descriptor_set.setDstBinding(binding_info.binding)
        .setDstArrayElement(index)
        .setDescriptorType(binding_info.descriptorType)
        .setImageInfo(image_infos);
    return *this;    
}

VulkanDescriptorSetUpdater & VulkanDescriptorSetUpdater::add(uint32_t binding, const vk::DescriptorBufferInfo &buffer_info)
{
    m_buffer_info_list.emplace_back(buffer_info);
    return this->add(binding, 0, std::span<const vk::DescriptorBufferInfo>(&buffer_info, 1));
}

VulkanDescriptorSetUpdater & VulkanDescriptorSetUpdater::add(uint32_t binding, uint32_t index, const vk::DescriptorBufferInfo &buffer_info)
{
    m_buffer_info_list.emplace_back(buffer_info);
    return this->add(binding, index, std::span<const vk::DescriptorBufferInfo>(&buffer_info, 1));
}

VulkanDescriptorSetUpdater & VulkanDescriptorSetUpdater::add(uint32_t binding, std::span<const vk::DescriptorBufferInfo> buffer_infos)
{
    return this->add(binding, 0, buffer_infos);
}

VulkanDescriptorSetUpdater & VulkanDescriptorSetUpdater::add(uint32_t binding, uint32_t index, std::span<const vk::DescriptorBufferInfo> buffer_infos)
{
    const auto &binding_info = m_binding_span[binding];
    vk::WriteDescriptorSet & write_descriptor_set = m_write_descriptor_sets.emplace_back();
    write_descriptor_set.setDstBinding(binding_info.binding)
        .setDstArrayElement(index)
        .setDescriptorType(binding_info.descriptorType)
        .setBufferInfo(buffer_infos);
    return *this;    
}

void VulkanDescriptorSetUpdater::update()
{
    for (auto &write_descriptor_set : m_write_descriptor_sets) {
        write_descriptor_set.setDstSet(m_target_set);
    }
    m_device.updateDescriptorSets(m_write_descriptor_sets, nullptr);
    m_write_descriptor_sets.clear();
    m_image_info_list.clear();
    m_buffer_info_list.clear();
}