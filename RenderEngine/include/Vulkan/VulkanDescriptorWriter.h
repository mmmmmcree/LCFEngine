#pragma once

#include <span>
#include <vulkan/vulkan.hpp>

namespace lcf::render {
    class VulkanContext;

    class VulkanDescriptorWriter
    {
        using Self = VulkanDescriptorWriter;
    public:
        using DescriptorSetLayoutBindings = std::span<const vk::DescriptorSetLayoutBinding>;
        using WriteDescriptorSetList = std::vector<vk::WriteDescriptorSet>;
        VulkanDescriptorWriter(VulkanContext * context_p, DescriptorSetLayoutBindings binding_list);
        Self & add(uint32_t binding, const vk::DescriptorImageInfo & image_info);
        Self & add(uint32_t binding, uint32_t index, const vk::DescriptorImageInfo & image_info);
        Self & add(uint32_t binding, const vk::DescriptorBufferInfo & buffer_info);
        Self & add(uint32_t binding, uint32_t index, const vk::DescriptorBufferInfo & buffer_info);
        void write(vk::DescriptorSet set);
    private:
        VulkanContext * m_context_p;
        DescriptorSetLayoutBindings m_layout_binding_list;
        WriteDescriptorSetList m_write_descriptor_sets;
    };
}