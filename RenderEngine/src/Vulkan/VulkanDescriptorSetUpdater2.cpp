#include "Vulkan/VulkanDescriptorSetUpdater2.h"
#include <ranges>

using namespace lcf::render;

VulkanDescriptorSetUpdater2::VulkanDescriptorSetUpdater2(
    vk::Device device,
    vk::DescriptorSet target_set,
    BindingReadSpan binding_span)
    : m_device(device)
    , m_target_set(target_set)
    , m_binding_span(binding_span)
{
}

const VulkanDescriptorSetBinding *
VulkanDescriptorSetUpdater2::findBinding(uint32_t binding) const noexcept
{
    auto it = std::ranges::find_if(m_binding_span, [binding](const auto & b) {
        return b.binding == binding;
    });
    return it != m_binding_span.end() ? &*it : nullptr;
}

VulkanDescriptorSetUpdater2 &
VulkanDescriptorSetUpdater2::add(uint32_t binding, const vk::DescriptorImageInfo & image_info)
{
    m_image_info_list.emplace_back(image_info);
    return this->add(binding, 0u, std::span<const vk::DescriptorImageInfo>{&m_image_info_list.back(), 1u});
}

VulkanDescriptorSetUpdater2 &
VulkanDescriptorSetUpdater2::add(uint32_t binding, uint32_t index, const vk::DescriptorImageInfo & image_info)
{
    m_image_info_list.emplace_back(image_info);
    return this->add(binding, index, std::span<const vk::DescriptorImageInfo>{&m_image_info_list.back(), 1u});
}

VulkanDescriptorSetUpdater2 &
VulkanDescriptorSetUpdater2::add(uint32_t binding, std::span<const vk::DescriptorImageInfo> image_infos)
{
    return this->add(binding, 0u, image_infos);
}

VulkanDescriptorSetUpdater2 &
VulkanDescriptorSetUpdater2::add(uint32_t binding, uint32_t index, std::span<const vk::DescriptorImageInfo> image_infos)
{
    auto * b = this->findBinding(binding);
    if (not b) { return *this; }
    auto & write = m_write_list.emplace_back();
    write.setDstBinding(b->binding)
         .setDstArrayElement(index)
         .setDescriptorType(b->descriptorType)
         .setImageInfo(image_infos);
    return *this;
}

VulkanDescriptorSetUpdater2 &
VulkanDescriptorSetUpdater2::add(uint32_t binding, const vk::DescriptorBufferInfo & buffer_info)
{
    m_buffer_info_list.emplace_back(buffer_info);
    return this->add(binding, 0u, std::span<const vk::DescriptorBufferInfo>{&m_buffer_info_list.back(), 1u});
}

VulkanDescriptorSetUpdater2 &
VulkanDescriptorSetUpdater2::add(uint32_t binding, uint32_t index, const vk::DescriptorBufferInfo & buffer_info)
{
    m_buffer_info_list.emplace_back(buffer_info);
    return this->add(binding, index, std::span<const vk::DescriptorBufferInfo>{&m_buffer_info_list.back(), 1u});
}

VulkanDescriptorSetUpdater2 &
VulkanDescriptorSetUpdater2::add(uint32_t binding, std::span<const vk::DescriptorBufferInfo> buffer_infos)
{
    return this->add(binding, 0u, buffer_infos);
}

VulkanDescriptorSetUpdater2 &
VulkanDescriptorSetUpdater2::add(uint32_t binding, uint32_t index, std::span<const vk::DescriptorBufferInfo> buffer_infos)
{
    auto * b = this->findBinding(binding);
    if (not b) { return *this; }
    auto & write = m_write_list.emplace_back();
    write.setDstBinding(b->binding)
         .setDstArrayElement(index)
         .setDescriptorType(b->descriptorType)
         .setBufferInfo(buffer_infos);
    return *this;
}

void VulkanDescriptorSetUpdater2::update()
{
    for (auto & write : m_write_list) {
        write.setDstSet(m_target_set);
    }
    m_device.updateDescriptorSets(m_write_list, nullptr);
    m_write_list.clear();
    m_image_info_list.clear();
    m_buffer_info_list.clear();
}
