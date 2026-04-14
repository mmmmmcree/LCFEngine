#pragma once

#include <vulkan/vulkan.hpp>
#include "Vulkan/vulkan_fwd_decls.h"
#include "resource_utils.h"
#include "interval/interval_containers.h"
#include <unordered_map>

namespace lcf::render {
    struct MemoryAllocationCreateInfo;

    class VulkanImageProxy
    {
        using Self = VulkanImageProxy;
        friend class VulkanImageObject;
    public:
        struct ImageViewKey
        {
            ImageViewKey(const vk::ImageSubresourceRange & range, const vk::ComponentMapping & components = {}) :
                m_range(range), m_components(components) {}
            bool operator==(const ImageViewKey & other) const noexcept;
            vk::ImageSubresourceRange m_range;
            vk::ComponentMapping m_components;
        };
    private:
        struct ImageViewKeyHash
        {
            std::size_t operator()(const ImageViewKey & key) const noexcept;
        };
        using ImageViewMap = std::unordered_map<ImageViewKey, vk::UniqueImageView, ImageViewKeyHash>;
        using LayoutMap = icl::interval_map<uint16_t, icl::DefaultIntervalWrapper<vk::ImageLayout>>;
        using LayoutMapInterval = typename LayoutMap::interval_type;
    public:
        VulkanImageProxy() = default;
        ~VulkanImageProxy();
        VulkanImageProxy(const Self &) = delete;
        Self & operator=(const Self &) = delete;
        VulkanImageProxy(Self &&) noexcept = default;
        Self & operator=(Self &&) noexcept = default;
    public:
        Self & addImageFlags(vk::ImageCreateFlags flags) noexcept { m_flags |= flags; return *this; }
        Self & setFormat(vk::Format format) noexcept { m_format = format; return *this; }
        Self & setExtent(vk::Extent3D extent) noexcept { m_extent = extent; return *this; }
        Self & setMipmapped(bool mipmapped) noexcept { m_mip_level_count = !mipmapped; return *this; }
        Self & setArrayLayers(uint16_t array_layers) noexcept { m_array_layers = array_layers; return *this; }
        Self & setSamples(vk::SampleCountFlagBits samples) noexcept { m_samples = samples; return *this; }
        Self & setUsage(vk::ImageUsageFlags usage) noexcept { m_usage = usage; return *this; }
        ResourceLease lease() const noexcept { return m_image_rp.lease(); }
        bool isCreated() const noexcept { return m_image_rp and this->getHandle(); }
        vk::Image getHandle() const noexcept;
        std::span<std::byte> getMappedMemorySpan() const noexcept;
        vk::ImageView getDefaultView() const;
        vk::ImageView getView(const ImageViewKey & image_view_key) const;
        vk::ImageAspectFlags getAspectFlags() const noexcept;
        vk::ImageType getImageType() const noexcept;
        vk::ImageCreateFlags getImageFlags() const { return m_flags; }
        vk::Format getFormat() const { return m_format; }
        vk::Extent3D getExtent() const { return m_extent; }
        uint32_t getMipLevelCount() const noexcept { return m_mip_level_count; }
        uint32_t getArrayLayerCount() const { return m_array_layers; }
        vk::SampleCountFlagBits getSamples() const { return m_samples; }
        void transitLayout(VulkanCommandBufferObject & cmd, vk::ImageLayout new_layout);
        void transitLayout(VulkanCommandBufferObject & cmd, const vk::ImageSubresourceRange & subresource_range, vk::ImageLayout new_layout);
        void copyFrom(VulkanCommandBufferObject & cmd, vk::Buffer buffer, std::span<const vk::BufferImageCopy> regions);
        std::optional<vk::ImageLayout> getLayout() const noexcept { return this->getLayout(0, m_array_layers, 0, m_mip_level_count); }
        std::optional<vk::ImageLayout> getLayout(uint32_t base_layer, uint32_t layer_count, uint32_t base_mip_level, uint32_t mip_level_count) const noexcept;
        vk::ImageSubresourceRange getFullResourceRange() const noexcept;
        void blitTo(VulkanCommandBufferObject & cmd,
            const vk::ImageSubresourceLayers & src_subresource,
            const std::pair<vk::Offset3D, vk::Offset3D> & src_offsets,
            VulkanImageProxy & dst,
            const vk::ImageSubresourceLayers & dst_subresource,
            const std::pair<vk::Offset3D, vk::Offset3D> & dst_offsets,
            vk::Filter filter);
        void copyTo(VulkanCommandBufferObject & cmd,
            const vk::ImageSubresourceLayers & src_subresource,
            const vk::Offset3D & src_offset,
            VulkanImageProxy & dst,
            const vk::ImageSubresourceLayers & dst_subresource,
            const vk::Offset3D & dst_offset,
            const vk::Extent3D & extent);
    private:
        bool _create(VulkanContext * context_p, vk::ImageTiling tiling, const MemoryAllocationCreateInfo & memory_info);
        vk::UniqueImageView generateView(const ImageViewKey & image_view_key) const;
        vk::ImageViewType deduceImageViewType(const vk::ImageSubresourceRange & subresource_range) const noexcept;
        uint32_t getLayoutKey(uint32_t layer, uint32_t mip_level) const noexcept { return layer * this->getMipLevelCount() + mip_level; }
        std::vector<LayoutMapInterval> getLayoutIntervals(uint32_t base_layer, uint32_t layer_count, uint32_t base_mip_level, uint32_t mip_level_count) const noexcept;
    private:
        VulkanContext * m_context_p = nullptr;
        vk::ImageCreateFlags m_flags = {};
        vk::Format m_format = vk::Format::eUndefined;
        vk::Extent3D m_extent = {};
        uint16_t m_mip_level_count = 1u;
        uint16_t m_array_layers = 1u;
        vk::SampleCountFlagBits m_samples = vk::SampleCountFlagBits::e1;
        vk::ImageUsageFlags m_usage = {};
        ResourcePointer<VulkanImage> m_image_rp;
        mutable ImageViewMap m_view_map;
        LayoutMap m_layout_map;
    };
}
