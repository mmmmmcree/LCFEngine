#pragma once

#include <vulkan/vulkan.hpp>
#include "vulkan_fwd_decls.h"
#include <optional>

namespace lcf::render {
    class VulkanAttachment
    {
        using Self = VulkanAttachment;
    public:
        using Offset3DPair = std::pair<vk::Offset3D, vk::Offset3D>;
        VulkanAttachment(const VulkanImageObjectSharedPointer & image_sp);
        VulkanAttachment(const VulkanImageObjectSharedPointer & image_sp, uint32_t mip_level, uint32_t layer, uint32_t layer_count);
        ~VulkanAttachment() noexcept;
        VulkanAttachment(const VulkanAttachment & other) = default;
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
        const VulkanImageObjectSharedPointer & getImageSharedPointer() const noexcept { return m_image_sp; }
        vk::ImageSubresourceRange getSubresourceRange() const noexcept;
        vk::ImageView getImageView() const noexcept;
        uint32_t getMipLevel() const noexcept { return m_mip_level; }
        uint32_t getLayer() const noexcept { return m_layer; }
        uint32_t getLayerCount() const noexcept { return m_layer_count; }
        void transitLayout(VulkanCommandBufferObject & cmd, vk::ImageLayout new_layout);
        vk::Extent3D getExtent() const noexcept;
        std::optional<vk::ImageLayout> getLayout() const noexcept;
    private:
        VulkanImageObjectSharedPointer m_image_sp;
        uint32_t m_mip_level;
        uint32_t m_layer;
        uint32_t m_layer_count;
    };
}