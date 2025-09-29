#pragma once

#include <vulkan/vulkan.hpp>
#include "VulkanMemoryAllocator.h"
#include "Image/Image.h"
#include "PointerDefs.h"
#include "VulkanCommandBufferObject.h"
#include <variant>
#include <unordered_map>

namespace lcf::render {
    class VulkanContext;

    class VulkanAttachment;

    class VulkanImage : public STDPointerDefs<VulkanImage>
    {
        using Self = VulkanImage;
        struct ImageViewKey
        {
            ImageViewKey(const vk::ImageSubresourceRange & range, const vk::ComponentMapping & components = {}) :
                m_range(range), m_components(components) {}
            bool operator==(const ImageViewKey & other) const noexcept
            {
                return m_range == other.m_range and m_components == other.m_components;
            }
            vk::ImageSubresourceRange m_range;
            vk::ComponentMapping m_components;
        };
        struct ImageViewKeyHash
        {
            std::size_t operator()(const ImageViewKey & key) const noexcept
            {
                uint64_t packed = 0;
                packed |= static_cast<uint64_t>(static_cast<uint32_t>(key.m_range.aspectMask)) << 56;
                packed |= (static_cast<uint64_t>(key.m_range.baseMipLevel) & 0xFF) << 48;
                packed |= (static_cast<uint64_t>(key.m_range.levelCount) & 0xFF) << 40;
                packed |= (static_cast<uint64_t>(key.m_range.baseArrayLayer) & 0xFFFF) << 32;
                packed |= (static_cast<uint64_t>(key.m_range.layerCount) & 0xFFFF) << 24;
                packed |= (static_cast<uint64_t>(static_cast<uint32_t>(key.m_components.r)) & 0xFF) << 20;
                packed |= (static_cast<uint64_t>(static_cast<uint32_t>(key.m_components.g)) & 0xFF) << 16;
                packed |= (static_cast<uint64_t>(static_cast<uint32_t>(key.m_components.b)) & 0xFF) << 12;
                packed |= (static_cast<uint64_t>(static_cast<uint32_t>(key.m_components.a)) & 0xFF) << 8;
                return std::hash<uint64_t>{}(packed);
            }
        };
        using ImageVariant = std::variant<vk::Image, VMAImage::UniquePointer>;
        using ImageViewMap = std::unordered_map<ImageViewKey, vk::UniqueImageView, ImageViewKeyHash>;
        friend class VulkanAttachment;
    public:
        VulkanImage() = default;
        VulkanImage(const VulkanImage & other) = delete;
        VulkanImage & operator=(const VulkanImage & other) = delete;
        bool create(VulkanContext * context_p);
        bool create(VulkanContext * context_p, vk::Image external_image);
        bool create(VulkanContext * context_p, const Image & image); 
        bool isCreated() const noexcept { return this->getHandle(); }
        void transitLayout(VulkanCommandBufferObject * cmd_p, vk::ImageLayout new_layout);
        void transitLayout(VulkanCommandBufferObject * cmd_p, const vk::ImageSubresourceRange & subresource_range, vk::ImageLayout new_layout);
        vk::Image getHandle() const;
        vk::ImageView getDefaultView() const;
        vk::ImageView getView(const vk::ImageSubresourceRange & subresource_range) const;
        Self & setImageType(vk::ImageType image_type) { m_image_type = image_type; return *this; }
        vk::ImageType getImageType() const { return m_image_type; }
        Self & setFormat(vk::Format format) { m_format = format; return *this; }
        vk::Format getFormat() const { return m_format; }
        Self & setExtent(vk::Extent3D extent) { m_extent = extent; return *this; }
        vk::Extent3D getExtent() const { return m_extent; }
        Self & setMipmapped(bool mipmapped) { m_mipmapped = mipmapped; return *this; }
        uint32_t getMipLevelCount() const;
        Self & setArrayLayers(uint32_t array_layers) { m_array_layers = array_layers; return *this; }
        uint32_t getArrayLayerCount() const { return m_array_layers; }
        Self & setSamples(vk::SampleCountFlagBits samples) { m_samples = samples; return *this; }
        vk::SampleCountFlagBits getSamples() const { return m_samples; }
        Self & setTiling(vk::ImageTiling tiling) { m_tiling = tiling; return *this; }
        vk::ImageTiling getTiling() const { return m_tiling; }
        Self & setUsage(vk::ImageUsageFlags usage) { m_usage = usage; return *this; }
        vk::ImageUsageFlags getUsage() const { return m_usage; }
        Self & setInitialLayout(vk::ImageLayout layout) { m_layout = layout; return *this; }
        vk::ImageLayout getLayout() const { return m_layout; }
    private:
        vk::UniqueImageView generateView(const ImageViewKey & image_view_key) const;
        vk::ImageAspectFlags getAspectFlags() const noexcept;
        vk::ImageViewType deduceImageViewType(const vk::ImageSubresourceRange & subresource_range) const noexcept;
        vk::ImageSubresourceRange getFullResourceRange() const noexcept;
    private:
        VulkanContext * m_context_p;
        vk::ImageCreateFlags m_flags = {};
        vk::ImageType m_image_type = vk::ImageType::e2D;
        vk::Format m_format = vk::Format::eUndefined;
        vk::Extent3D m_extent = {};
        bool m_mipmapped = false;
        uint32_t m_array_layers = 1u;
        vk::SampleCountFlagBits m_samples = vk::SampleCountFlagBits::e1;
        vk::ImageTiling m_tiling = vk::ImageTiling::eOptimal;
        vk::ImageUsageFlags m_usage = {};
        vk::ImageLayout m_layout = vk::ImageLayout::eUndefined;
        ImageVariant m_image;
        mutable ImageViewMap m_view_map;
    };

    class VulkanAttachment
    {
        using Self = VulkanAttachment;
    public:
        using Offset3DPair = std::pair<vk::Offset3D, vk::Offset3D>;
        VulkanAttachment(const VulkanImage::SharedPointer & image_p,
            uint32_t mip_level = 0,
            uint32_t layer = 0,
            uint32_t layer_count = 1);
        VulkanAttachment(const VulkanAttachment & other) = default;
        void blitTo(VulkanCommandBufferObject * cmd_p,
            VulkanAttachment & dst,
            vk::Filter filter,
            const Offset3DPair & src_offsets,
            const Offset3DPair & dst_offsets);
        void copyTo(VulkanCommandBufferObject * cmd_p,
            VulkanAttachment & dst,
            const vk::Offset3D & src_offset,
            const vk::Offset3D & dst_offset,
            const vk::Extent3D & extent);
        const VulkanImage::SharedPointer & getImageSharedPointer() const noexcept { return m_image_sp; }
        vk::ImageSubresourceRange getSubresourceRange() const noexcept;
        vk::ImageView getImageView() const noexcept;
        uint32_t getMipLevel() const noexcept { return m_mip_level; }
        uint32_t getLayer() const noexcept { return m_layer; }
        uint32_t getLayerCount() const noexcept { return m_layer_count; }
        Self & setInitialLayout(vk::ImageLayout layout) { m_image_sp->setInitialLayout(layout); return *this; }
        void transitLayout(VulkanCommandBufferObject * cmd_p, vk::ImageLayout new_layout);
        vk::ImageLayout getLayout() const noexcept;
    private:
        VulkanImage::SharedPointer m_image_sp;
        uint32_t m_mip_level;
        uint32_t m_layer;
        uint32_t m_layer_count;
    };
}