#pragma once

#include "vk_core/memory/Image.h"
#include <vulkan/vulkan.hpp>
#include <vector>
#include <utility>
#include <span>

namespace lcf::vkc {

class MemoryAllocator; 

class RenderTargetCreateInfo;

class RenderTarget
{
    using ClearValueList = std::vector<vk::ClearValue>;
public:
    std::error_code create(const MemoryAllocator & allocator, const RenderTargetCreateInfo & create_info) noexcept;
    void setRenderArea(const vk::Rect2D & render_area) noexcept;
    void setColorAttachmentClearValue(const vk::ClearColorValue & clear_value);
    void setDepthStencilAttachmentClearValue(const vk::ClearDepthStencilValue & clear_value);
    const vk::Rect2D & getRenderArea() const noexcept { return m_render_area; }
    const ClearValueList & getClearValues() const noexcept { return m_clear_values; }
    const uint32_t & getLayerCount() const noexcept { return m_layer_count; }
private:
    vk::Rect2D m_render_area;
    ClearValueList m_clear_values;
    uint32_t m_layer_count = 1;
};

class RenderTargetCreateInfo
{
    using Self = RenderTargetCreateInfo;
    using FormatList = std::vector<vk::Format>;
public:
    ~RenderTargetCreateInfo() noexcept = default;
    RenderTargetCreateInfo(
        vk::Extent2D extent = {},
        vk::Format depth_stencil_format = vk::Format::eUndefined,
        vk::SampleCountFlagBits sample_count = vk::SampleCountFlagBits::e1,
        vk::ResolveModeFlagBits resolve_mode = vk::ResolveModeFlagBits::eNone) noexcept :
        m_extent(extent),
        m_depth_stencil_format(depth_stencil_format),
        m_sample_count(sample_count),
        m_resolve_mode(resolve_mode) {}
    RenderTargetCreateInfo(const Self & other) noexcept = default;
    RenderTargetCreateInfo(Self && other) noexcept = default;
    Self & operator=(const Self & other) noexcept = default;
    Self & operator=(Self && other) noexcept = default;
public:
    Self & setExtent(const vk::Extent2D & extent) noexcept { m_extent = extent; return *this; }
    Self & addColorFormat(vk::Format format) noexcept { m_color_formats.emplace_back(format); return *this; }
    Self & addColorFormats(std::span<const vk::Format> formats) noexcept { m_color_formats.append_range(formats); return *this; }
    Self & setSampleCount(vk::SampleCountFlagBits sample_count) noexcept { m_sample_count = sample_count; return *this; }
    Self & setDepthStencilFormat(vk::Format depth_stencil_format) noexcept { m_depth_stencil_format = depth_stencil_format; return *this; }
    Self & setResolveMode(vk::ResolveModeFlagBits resolve_mode) noexcept { m_resolve_mode = resolve_mode; return *this; }
    const vk::Extent2D & getExtent() const noexcept { return m_extent; }
    const FormatList & getColorFormats() const noexcept { return m_color_formats; }
    const vk::SampleCountFlagBits & getSampleCount() const noexcept { return m_sample_count; }
    bool hasDepthStencilFormat() const noexcept { return m_depth_stencil_format != vk::Format::eUndefined; }
    const vk::Format & getDepthStencilFormat() const noexcept { return m_depth_stencil_format; }
    bool isMSAAEnabled() const noexcept { return m_resolve_mode != vk::ResolveModeFlagBits::eNone and std::to_underlying(m_sample_count) > 1; }
    const vk::ResolveModeFlagBits & getResolveMode() const noexcept { return m_resolve_mode; }
private:
    vk::Extent2D m_extent;
    FormatList m_color_formats;
    vk::Format m_depth_stencil_format;
    vk::SampleCountFlagBits m_sample_count;
    vk::ResolveModeFlagBits m_resolve_mode;
};

} // namespace lcf::vkc
