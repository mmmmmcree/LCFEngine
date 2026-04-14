#pragma once

#include <vulkan/vulkan.hpp>
#include <optional>

namespace lcf::render {
    class VulkanImageObject;
    class VulkanCommandBufferObject;

    /// Non-owning sub-resource view into a VulkanImageObject.
    /// Selects a specific mip level + layer range, provides convenience
    /// accessors for image view, layout, extent, and GPU transfer operations.
    ///
    /// Lifetime contract: the referenced VulkanImageObject must outlive
    class VulkanAttachment
    {
        using Self = VulkanAttachment;
    public:
        using Offset3DPair = std::pair<vk::Offset3D, vk::Offset3D>;
        VulkanAttachment(VulkanImageObject & image);
        VulkanAttachment(VulkanImageObject & image, uint32_t mip_level, uint32_t layer, uint32_t layer_count);
        ~VulkanAttachment() noexcept;
        VulkanAttachment(const VulkanAttachment & other) = default;
        VulkanAttachment & operator=(const VulkanAttachment & other) = default;
        bool isValid() const noexcept;
        void blitTo(VulkanCommandBufferObject & cmd,
            VulkanAttachment & dst,
            vk::Filter filter,
            const Offset3DPair & src_offsets,
            const Offset3DPair & dst_offsets);
        void blitTo(VulkanCommandBufferObject & cmd, VulkanAttachment & dst, vk::Filter filter);
        void copyTo(VulkanCommandBufferObject & cmd,
            VulkanAttachment & dst,
            const vk::Offset3D & src_offset,
            const vk::Offset3D & dst_offset,
            const vk::Extent3D & extent);
        VulkanImageObject & getImageObject() const noexcept { return *m_image_p; }
        vk::ImageSubresourceRange getSubresourceRange() const noexcept;
        vk::ImageView getImageView() const noexcept;
        uint32_t getMipLevel() const noexcept { return m_mip_level; }
        uint32_t getLayer() const noexcept { return m_layer; }
        uint32_t getLayerCount() const noexcept { return m_layer_count; }
        void transitLayout(VulkanCommandBufferObject & cmd, vk::ImageLayout new_layout);
        vk::Extent3D getExtent() const noexcept;
        std::optional<vk::ImageLayout> getLayout() const noexcept;
    private:
        VulkanImageObject * m_image_p = nullptr;
        uint32_t m_mip_level = 0;
        uint32_t m_layer = 0;
        uint32_t m_layer_count = 1;
    };
}