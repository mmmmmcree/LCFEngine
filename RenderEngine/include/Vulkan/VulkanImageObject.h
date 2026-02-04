#pragma once

#include <vulkan/vulkan.hpp>
#include "vulkan_memory_resources.h"
#include "image/Image.h"
#include "PointerDefs.h"
#include <variant>
#include <unordered_map>
#include <boost/icl/interval_map.hpp>
#include <optional>

namespace lcf::render {
    class VulkanContext;

    class VulkanAttachment;

    class VulkanCommandBufferObject;

    //todo temporary
    template <typename T>
    class DefaultIntervalWrapper
    {
        using Self = DefaultIntervalWrapper<T>;
    public:
        DefaultIntervalWrapper() = default;
        DefaultIntervalWrapper(T && value) : m_value(std::forward<T>(value)) {}
        bool operator==(const Self & other) const { return m_value == other.m_value; }
        Self & operator+=(const Self & other) { return *this; }
        const T & getValue() const { return m_value.value(); }
    private:
        std::optional<T> m_value;
    };

    class VulkanImageObject : public STDPointerDefs<VulkanImageObject>
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
        using ImageVariant = std::variant<vk::Image, VulkanImage::SharedPointer>;
        using ImageViewMap = std::unordered_map<ImageViewKey, vk::UniqueImageView, ImageViewKeyHash>;
        using WrappedImageLayout = DefaultIntervalWrapper<vk::ImageLayout>;
        using LayoutMap = boost::icl::interval_map<uint16_t, WrappedImageLayout>;
        using LayoutMapInterval = typename boost::icl::interval<typename LayoutMap::domain_type>;
        using LayoutMapIntervalType = typename LayoutMapInterval::type;
        friend class VulkanAttachment;
    public:
        VulkanImageObject() = default;
        ~VulkanImageObject() = default;
        VulkanImageObject(const VulkanImageObject & other) = delete;
        VulkanImageObject & operator=(const VulkanImageObject & other) = delete;
        VulkanImageObject(VulkanImageObject && other) noexcept = default;
        VulkanImageObject & operator=(VulkanImageObject && other) noexcept = default;
        bool create(VulkanContext * context_p);
        bool create(VulkanContext * context_p, vk::Image external_image);
        void setData(VulkanCommandBufferObject & cmd, std::span<const std::byte> data, uint32_t layer = 0);
        Image readData();
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
        bool _create(VulkanContext * context_p, vk::ImageTiling tiling, MemoryAllocationCreateInfo memory_info);
        vk::UniqueImageView generateView(const ImageViewKey & image_view_key) const;
        vk::ImageViewType deduceImageViewType(const vk::ImageSubresourceRange & subresource_range) const noexcept;
        vk::ImageSubresourceRange getFullResourceRange() const noexcept;
        uint32_t getLayoutKey(uint32_t layer, uint32_t mip_level) const noexcept { return layer * this->getMipLevelCount() + mip_level; }
        std::vector<LayoutMapIntervalType> getLayoutIntervals(uint32_t base_layer, uint32_t layer_count, uint32_t base_mip_level, uint32_t mip_level_count) const noexcept;
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
        ImageVariant m_image;
        mutable ImageViewMap m_view_map;
        LayoutMap m_layout_map;
    };

    class VulkanAttachment
    {
        using Self = VulkanAttachment;
    public:
        using Offset3DPair = std::pair<vk::Offset3D, vk::Offset3D>;
        VulkanAttachment(const VulkanImageObject::SharedPointer & image_sp);
        VulkanAttachment(const VulkanImageObject::SharedPointer & image_sp, uint32_t mip_level, uint32_t layer, uint32_t layer_count);
        VulkanAttachment(const VulkanAttachment & other) = default;
        bool isValid() const noexcept { return m_image_sp and m_image_sp->isCreated(); }
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
        const VulkanImageObject::SharedPointer & getImageSharedPointer() const noexcept { return m_image_sp; }
        vk::ImageSubresourceRange getSubresourceRange() const noexcept;
        vk::ImageView getImageView() const noexcept;
        uint32_t getMipLevel() const noexcept { return m_mip_level; }
        uint32_t getLayer() const noexcept { return m_layer; }
        uint32_t getLayerCount() const noexcept { return m_layer_count; }
        void transitLayout(VulkanCommandBufferObject & cmd, vk::ImageLayout new_layout);
        vk::Extent3D getExtent() const noexcept;
        std::optional<vk::ImageLayout> getLayout() const noexcept { return m_image_sp->getLayout(m_layer, m_layer_count, m_mip_level, 1); }
    private:
        VulkanImageObject::SharedPointer m_image_sp;
        uint32_t m_mip_level;
        uint32_t m_layer;
        uint32_t m_layer_count;
    };
}