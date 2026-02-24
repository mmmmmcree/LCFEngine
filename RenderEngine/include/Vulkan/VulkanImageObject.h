#pragma once

#include <vulkan/vulkan.hpp>
#include "vulkan_fwd_decls.h"
#include <unordered_map>
#include "interval/interval_containers.h"

namespace lcf::render {
    class VulkanAttachment;

    struct MemoryAllocationCreateInfo;

    class VulkanImageObject : public VulkanImageObjectPointerDefs
    {
        using Self = VulkanImageObject;
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
        using ImageViewMap = std::unordered_map<ImageViewKey, vk::UniqueImageView, ImageViewKeyHash>;
        using LayoutMap = icl::interval_map<uint16_t, icl::DefaultIntervalWrapper<vk::ImageLayout>>;
        using LayoutMapInterval = typename LayoutMap::interval_type;
        friend class VulkanAttachment;
    public:
        VulkanImageObject() = default;
        ~VulkanImageObject();
        VulkanImageObject(const VulkanImageObject & other) = delete;
        VulkanImageObject & operator=(const VulkanImageObject & other) = delete;
        VulkanImageObject(VulkanImageObject && other) noexcept = default;
        VulkanImageObject & operator=(VulkanImageObject && other) noexcept = default;
        bool create(VulkanContext * context_p);
        bool create(VulkanContext * context_p, vk::Image external_image);
        void setData(VulkanCommandBufferObject & cmd, std::span<const std::byte> data, uint32_t layer = 0);
        void generateMipmaps(VulkanCommandBufferObject & cmd);
        bool isCreated() const noexcept { return this->getHandle(); }
        void transitLayout(VulkanCommandBufferObject & cmd, vk::ImageLayout new_layout);
        void transitLayout(VulkanCommandBufferObject & cmd, const vk::ImageSubresourceRange & subresource_range, vk::ImageLayout new_layout);
        void copyFrom(VulkanCommandBufferObject & cmd, vk::Buffer buffer, std::span<const vk::BufferImageCopy> regions);
        vk::Image getHandle() const noexcept;
        std::byte * getMappedMemoryPtr() const noexcept;
        vk::ImageView getDefaultView() const;
        vk::ImageView getView(const ImageViewKey & image_view_key) const;
        Self & addImageFlags(vk::ImageCreateFlags flags) { m_flags |= flags; return *this; }
        vk::ImageCreateFlags getImageFlags() const { return m_flags; }
        vk::ImageType getImageType() const noexcept;
        Self & setFormat(vk::Format format) { m_format = format; return *this; }
        vk::Format getFormat() const { return m_format; }
        Self & setExtent(vk::Extent3D extent) { m_extent = extent; return *this; }
        vk::Extent3D getExtent() const { return m_extent; }
        Self & setMipmapped(bool mipmapped) { m_mip_level_count = !mipmapped; return *this; }
        uint32_t getMipLevelCount() const noexcept { return m_mip_level_count; }
        Self & setArrayLayers(uint16_t array_layers) { m_array_layers = array_layers; return *this; }
        uint32_t getArrayLayerCount() const { return m_array_layers; }
        Self & setSamples(vk::SampleCountFlagBits samples) { m_samples = samples; return *this; }
        vk::SampleCountFlagBits getSamples() const { return m_samples; }
        Self & setUsage(vk::ImageUsageFlags usage) { m_usage = usage; return *this; }
        vk::ImageAspectFlags getAspectFlags() const noexcept;
        std::optional<vk::ImageLayout> getLayout() const noexcept { return this->getLayout(0, m_array_layers, 0, m_mip_level_count); }
    private:
        bool _create(VulkanContext * context_p, vk::ImageTiling tiling, const MemoryAllocationCreateInfo & memory_info);
        vk::UniqueImageView generateView(const ImageViewKey & image_view_key) const;
        vk::ImageViewType deduceImageViewType(const vk::ImageSubresourceRange & subresource_range) const noexcept;
        vk::ImageSubresourceRange getFullResourceRange() const noexcept;
        uint32_t getLayoutKey(uint32_t layer, uint32_t mip_level) const noexcept { return layer * this->getMipLevelCount() + mip_level; }
        std::vector<LayoutMapInterval> getLayoutIntervals(uint32_t base_layer, uint32_t layer_count, uint32_t base_mip_level, uint32_t mip_level_count) const noexcept;
        std::optional<vk::ImageLayout> getLayout(uint32_t base_layer, uint32_t layer_count, uint32_t base_mip_level, uint32_t mip_level_count) const noexcept;
        void blitTo(VulkanCommandBufferObject & cmd,
            const vk::ImageSubresourceLayers & src_subresource,
            const std::pair<vk::Offset3D, vk::Offset3D> & src_offsets,
            VulkanImageObject & dst,
            const vk::ImageSubresourceLayers & dst_subresource,
            const std::pair<vk::Offset3D, vk::Offset3D> & dst_offsets,
            vk::Filter filter);
        void copyTo(VulkanCommandBufferObject & cmd,
            const vk::ImageSubresourceLayers & src_subresource,
            const vk::Offset3D & src_offset,
            VulkanImageObject & dst,
            const vk::ImageSubresourceLayers & dst_subresource,
            const vk::Offset3D & dst_offset,
            const vk::Extent3D & extent);
    private:
        VulkanContext * m_context_p;
        vk::ImageCreateFlags m_flags = {};
        vk::Format m_format = vk::Format::eUndefined;
        vk::Extent3D m_extent = {};
        uint16_t m_mip_level_count = 1u;
        uint16_t m_array_layers = 1u;
        vk::SampleCountFlagBits m_samples = vk::SampleCountFlagBits::e1;
        vk::ImageUsageFlags m_usage = {};
        VulkanImageSharedPointer m_image_sp;
        mutable ImageViewMap m_view_map;
        LayoutMap m_layout_map;
    };

}