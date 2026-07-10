#pragma once

#include <vulkan/vulkan.hpp>
#include <type_traits>
#include <expected>
#include <system_error>
#include "resource_utils.h"

namespace lcf::vkc::details {

template <typename Handle>
requires std::is_same_v<Handle, vk::Image> or std::is_same_v<Handle, vk::Buffer>
class Memory;

} // namespace lcf::vkc::details

namespace lcf::vkc {

class MemoryAllocator;

class MemoryAllocationInfo;

class ImageDescription
{
    using Self = ImageDescription;
public:
    static constexpr uint64_t hash(const Self & self) noexcept
    {
        constexpr auto mix = [](uint64_t seed, uint64_t value) noexcept -> uint64_t {
            value += 0x9e3779b97f4a7c15ull;
            value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ull;
            value = (value ^ (value >> 27)) * 0x94d049bb133111ebull;
            value ^= value >> 31;
            return seed ^ (value + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2));
        };
        uint64_t h = 0ull;
        h = mix(h, static_cast<uint64_t>(self.m_type));
        h = mix(h, static_cast<uint64_t>(self.m_format));
        h = mix(h, static_cast<uint64_t>(self.m_sample_count));
        h = mix(h, static_cast<uint64_t>(static_cast<uint32_t>(self.m_usage_flags)));
        h = mix(h, static_cast<uint64_t>(self.m_extent.width));
        h = mix(h, static_cast<uint64_t>(self.m_extent.height));
        h = mix(h, static_cast<uint64_t>(self.m_extent.depth));
        h = mix(h, static_cast<uint64_t>(self.m_mip_level_count));
        h = mix(h, static_cast<uint64_t>(self.m_array_layer_count));
        return h;
    }
public:
    ImageDescription(
        vk::ImageType type = vk::ImageType::e2D,
        vk::Format format = vk::Format::eUndefined,
        vk::SampleCountFlagBits sample_count = vk::SampleCountFlagBits::e1,
        vk::ImageUsageFlags usage_flags = {},
        vk::Extent3D extent = {},
        uint32_t mip_level_count = 1u,
        uint32_t array_layer_count = 1u) noexcept :
        m_type(type),
        m_format(format),
        m_sample_count(sample_count),
        m_usage_flags(usage_flags),
        m_extent(extent),
        m_mip_level_count(mip_level_count),
        m_array_layer_count(array_layer_count) {}
    ImageDescription(const vk::ImageCreateInfo & info) noexcept :
        m_type(info.imageType),
        m_format(info.format),
        m_sample_count(info.samples),
        m_usage_flags(info.usage),
        m_extent(info.extent),
        m_mip_level_count(info.mipLevels),
        m_array_layer_count(info.arrayLayers) {}
    bool operator==(const ImageDescription &) const noexcept = default;
public:
    Self & setType(vk::ImageType type) noexcept { m_type = type; return *this; }
    Self & setFormat(vk::Format format) noexcept { m_format = format; return *this; }
    Self & setSampleCount(vk::SampleCountFlagBits sample_count) noexcept { m_sample_count = sample_count; return *this; }
    Self & addUsageFlags(vk::ImageUsageFlags usage_flags) noexcept { m_usage_flags |= usage_flags; return *this; }
    Self & setExtent(vk::Extent3D extent) noexcept { m_extent = extent; return *this; }
    Self & setMipLevelCount(uint32_t count) noexcept { m_mip_level_count = count; return *this; }
    Self & setArrayLayerCount(uint32_t count) noexcept { m_array_layer_count = count; return *this; }
    const vk::ImageType & getType() const noexcept { return m_type; }
    const vk::Format & getFormat() const noexcept { return m_format; }
    const vk::SampleCountFlagBits & getSampleCount() const noexcept { return m_sample_count; }
    const vk::ImageUsageFlags & getUsageFlags() const noexcept { return m_usage_flags; }
    const vk::Extent3D & getExtent() const noexcept { return m_extent; }
    const uint32_t & getMipLevelCount() const noexcept { return m_mip_level_count; }
    const uint32_t & getArrayLayerCount() const noexcept { return m_array_layer_count; }
private:
    vk::ImageType m_type;
    vk::Format m_format;
    vk::SampleCountFlagBits m_sample_count;
    vk::ImageUsageFlags m_usage_flags;
    vk::Extent3D m_extent;
    uint32_t m_mip_level_count;
    uint32_t m_array_layer_count; 
};

class Image
{
    using Self = Image;
    using Memory = details::Memory<vk::Image>;
public:
    ~Image() noexcept = default;
    Image() = default;
    Image(const Self &) noexcept = default;
    Self & operator=(const Self &) noexcept = default;
    Image(Self &&) noexcept = default;
    Self & operator=(Self &&) noexcept = default;
    operator vk::Image() const noexcept;
public:
    std::error_code create(
        const MemoryAllocator & allocator,
        const vk::ImageCreateInfo & image_info,
        const MemoryAllocationInfo & alloc_info) noexcept;
    const vk::Image & handle() const noexcept;
    ResourceLease lease() const noexcept;
    const ImageDescription & getDescription() const noexcept { return m_desc; }
    std::expected<vk::UniqueImageView, std::error_code> createView(
        const vk::ImageSubresourceRange & range,
        vk::ImageViewType view_type) const noexcept;
private:
    ResourcePtr<Memory> m_memory_rp;
    vk::Device m_device;
    ImageDescription m_desc;
};

class AttachmentDescription
{
    using Self = AttachmentDescription;
public:
    AttachmentDescription(
        uint32_t base_mip_level = 0u,
        uint32_t base_array_layer = 0u,
        uint32_t array_layer_count = 1u,
        vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eNone) noexcept :
        m_base_mip_level(base_mip_level),
        m_base_array_layer(base_array_layer),
        m_array_layer_count(array_layer_count),
        m_aspect_flags(aspect) {}
    bool operator==(const AttachmentDescription &) const noexcept = default;
public:
    Self & setBaseMipLevel(uint32_t level) noexcept { m_base_mip_level = level; return *this; }
    Self & setBaseArrayLayer(uint32_t layer) noexcept { m_base_array_layer = layer; return *this; }
    Self & setArrayLayerCount(uint32_t count) noexcept { m_array_layer_count = count; return *this; }
    Self & addAspectFlags(vk::ImageAspectFlags aspect) noexcept { m_aspect_flags |= aspect; return *this; }
    const uint32_t & getBaseMipLevel() const noexcept { return m_base_mip_level; }
    const uint32_t & getBaseArrayLayer() const noexcept { return m_base_array_layer; }
    const uint32_t & getArrayLayerCount() const noexcept { return m_array_layer_count; }
    const vk::ImageAspectFlags & getAspectFlags() const noexcept { return m_aspect_flags; }
    vk::ImageSubresourceRange getSubresourceRange() const noexcept
    {
        return { m_aspect_flags, m_base_mip_level, 1u, m_base_array_layer, m_array_layer_count };
    }
    vk::ImageViewType getViewType() const noexcept
    {
        return m_array_layer_count > 1u ? vk::ImageViewType::e2DArray : vk::ImageViewType::e2D;
    }
private:
    uint32_t m_base_mip_level;
    uint32_t m_base_array_layer;
    uint32_t m_array_layer_count;
    vk::ImageAspectFlags m_aspect_flags;
};

class Attachment
{
    using Self = Attachment;
public:
    ~Attachment() noexcept = default;
    Attachment() noexcept = default;
    Attachment(const Self &) = delete;
    Self & operator=(const Self &) = delete;
    Attachment(Self &&) noexcept = default;
    Self & operator=(Self &&) noexcept = default;
public:
    std::error_code create(const Image & image, const AttachmentDescription & desc) noexcept;
    const Image & getImage() const noexcept { return m_image; }
    const vk::ImageView & getImageView() const noexcept { return m_view.get(); }
    const AttachmentDescription & getDescription() const noexcept { return m_desc; }
private:
    Image m_image;
    vk::UniqueImageView m_view;
    AttachmentDescription m_desc;
};

} // namespace lcf::vkc
