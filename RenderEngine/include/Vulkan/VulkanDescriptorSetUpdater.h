#pragma once

#include <span>
#include <vulkan/vulkan.hpp>

namespace lcf::render {
    class VulkanContext;

    class VulkanDescriptorSetUpdater
    {
        using Self = VulkanDescriptorSetUpdater;
    public:
        using BindingReadSpan = std::span<const vk::DescriptorSetLayoutBinding>;
        using WriteDescriptorSetList = std::vector<vk::WriteDescriptorSet>;
        VulkanDescriptorSetUpdater(VulkanContext * context_p, vk::DescriptorSet target_set, BindingReadSpan binding_span);
        VulkanDescriptorSetUpdater(const Self &) = delete;
        Self & operator=(const Self &) = delete;
        VulkanDescriptorSetUpdater(Self &&) = default;
        Self & operator=(Self &&) = default;
        Self & setIndexOffset(uint32_t index_offset) { m_index_offset = index_offset; return *this; }
        Self & add(uint32_t binding, const vk::DescriptorImageInfo & image_info);
        Self & add(uint32_t binding, uint32_t index, const vk::DescriptorImageInfo & image_info);
        Self & add(uint32_t binding, const vk::DescriptorBufferInfo & buffer_info);
        Self & add(uint32_t binding, uint32_t index, const vk::DescriptorBufferInfo & buffer_info);
        void update();
    private:
        VulkanContext * m_context_p;
        vk::DescriptorSet m_target_set;
        BindingReadSpan m_binding_span;
        uint32_t m_index_offset = 0;
        WriteDescriptorSetList m_write_descriptor_sets;
    };
}