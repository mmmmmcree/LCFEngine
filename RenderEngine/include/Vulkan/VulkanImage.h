#pragma once

#include <vulkan/vulkan.hpp>
#include "VulkanMemoryAllocator.h"
#include "Image/Image.h"
#include "PointerDefs.h"
#include <unordered_map>

namespace lcf::render {
    class VulkanContext;

    class VulkanImage : public PointerDefs<VulkanImage>
    {
        using Self = VulkanImage;
        struct ImageSubresourceRangeHash {
            std::size_t operator()(const vk::ImageSubresourceRange & range) const
            {
                uint64_t packed = 0;
                packed |= static_cast<uint64_t>(static_cast<uint32_t>(range.aspectMask)) << 56;
                packed |= (static_cast<uint64_t>(range.baseMipLevel) & 0xFF) << 48;
                packed |= (static_cast<uint64_t>(range.levelCount) & 0xFF) << 40;
                packed |= (static_cast<uint64_t>(range.baseArrayLayer) & 0xFFFF) << 24;
                packed |= (static_cast<uint64_t>(range.layerCount) & 0xFFFF) << 8;
                return std::hash<uint64_t>{}(packed);
            }
        };
        using ImageViewMap = std::unordered_map<
            vk::ImageSubresourceRange,
            vk::UniqueImageView,
            ImageSubresourceRangeHash
        >;
    public:
        VulkanImage(VulkanContext * context);
        void create();
        void create(const vk::ImageCreateInfo & info);
        void setData(const Image & image); 
        void transitLayout(vk::ImageLayout new_layout);
        vk::Image getHandle() const { return m_image.get(); }
        vk::ImageView getDefaultView() const;
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
        Self & setLayout(vk::ImageLayout layout) { m_layout = layout; return *this; }
        vk::ImageLayout getLayout() const { return m_layout; }
    private:
        vk::UniqueImageView generateView(const vk::ImageSubresourceRange & subresource_range) const;
        vk::ImageAspectFlags getAspectFlags() const;
        vk::ImageViewType deduceImageViewType() const;
    private:
        VulkanContext * m_context;
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
        VMAUniqueImage m_image;
        mutable ImageViewMap m_view_map;
    };
}