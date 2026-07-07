#pragma once

#include <vulkan/vulkan.hpp>
#include <type_traits>
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
    static constexpr uint64_t hash(const Self & self) noexcept;
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
    const ImageDescription & description() const noexcept { return m_desc; }
private:
    ResourcePtr<Memory> m_memory_rp;
    vk::Device m_device;
    ImageDescription m_desc;
};

class AttachmentDescription
{};

class Attachment
{
    using Self = Attachment;
    using Memory = details::Memory<vk::Image>;
private:
    ResourcePtr<Memory> m_memory_rp;
    vk::UniqueImageView m_view;
    AttachmentDescription m_desc;
};

} // namespace lcf::vkc
