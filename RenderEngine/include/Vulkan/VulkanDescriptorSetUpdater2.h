#pragma once

#include <vulkan/vulkan.hpp>
#include "VulkanDescriptorSetBinding.h"
#include <span>
#include <vector>

namespace lcf::render {

    class VulkanDescriptorSetUpdater2
    {
        using Self = VulkanDescriptorSetUpdater2;
    public:
        using BindingReadSpan        = std::span<const VulkanDescriptorSetBinding>;
        using WriteDescriptorSetList = std::vector<vk::WriteDescriptorSet>;
        using ImageInfoList          = std::vector<vk::DescriptorImageInfo>;
        using BufferInfoList         = std::vector<vk::DescriptorBufferInfo>;

        VulkanDescriptorSetUpdater2(vk::Device device,
                                    vk::DescriptorSet target_set,
                                    BindingReadSpan binding_span);
        VulkanDescriptorSetUpdater2(const Self &) = delete;
        Self & operator=(const Self &) = delete;
        VulkanDescriptorSetUpdater2(Self &&) = default;
        Self & operator=(Self &&) = default;

        Self & add(uint32_t binding, const vk::DescriptorImageInfo & image_info);
        Self & add(uint32_t binding, uint32_t index, const vk::DescriptorImageInfo & image_info);
        Self & add(uint32_t binding, std::span<const vk::DescriptorImageInfo> image_infos);
        Self & add(uint32_t binding, uint32_t index, std::span<const vk::DescriptorImageInfo> image_infos);

        Self & add(uint32_t binding, const vk::DescriptorBufferInfo & buffer_info);
        Self & add(uint32_t binding, uint32_t index, const vk::DescriptorBufferInfo & buffer_info);
        Self & add(uint32_t binding, std::span<const vk::DescriptorBufferInfo> buffer_infos);
        Self & add(uint32_t binding, uint32_t index, std::span<const vk::DescriptorBufferInfo> buffer_infos);

        void update();

    private:
        const VulkanDescriptorSetBinding * findBinding(uint32_t binding) const noexcept;

        vk::Device        m_device;
        vk::DescriptorSet m_target_set;
        BindingReadSpan   m_binding_span;
        WriteDescriptorSetList m_write_list;
        ImageInfoList          m_image_info_list;
        BufferInfoList         m_buffer_info_list;
    };

}
