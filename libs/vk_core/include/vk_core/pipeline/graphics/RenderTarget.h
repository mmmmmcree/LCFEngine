#pragma once

#include "vk_core/memory/Image.h"
#include <vulkan/vulkan.hpp>
#include <vector>
#include <utility>
#include <span>
#include <optional>
#include <ranges>

namespace lcf::vkc {

class MemoryAllocator; 

class RenderTargetInfo;

class RenderTarget
{
    using Self = RenderTarget;
    using AttachmentList = std::vector<Attachment>;
    using ClearValueList = std::vector<vk::ClearValue>;
public:
    std::error_code build(const RenderTargetInfo & info) noexcept;
    std::error_code setColorAttachment(uint32_t index, const Image & image, uint32_t mip_level = 0, uint32_t array_layer = 0) noexcept;
    // std::error_code setDepthStencilAttachment(const Image & image, uint32_t mip_level = 0, uint32_t array_layer = 0) noexcept;
    Self & setRenderArea(const vk::Rect2D & render_area) noexcept { m_render_area = render_area; return *this; }
    const vk::Extent2D & getMaxExtent() const noexcept { return m_max_extent; }
    const vk::Rect2D & getRenderArea() const noexcept { return m_render_area; }
    const uint32_t & getLayerCount() const noexcept { return m_layer_count; }
    const Attachment & getAttachment(uint32_t index) const noexcept { return m_attachments[index]; }
    auto viewAttachmentImageViews() const noexcept { return m_attachments | std::views::transform(&Attachment::getImageView); }
    const ClearValueList & getClearValues() const noexcept { return m_clear_values; }
private:
    vk::Extent2D m_max_extent;
    vk::Rect2D m_render_area;
    uint32_t m_layer_count = 1;
    AttachmentList m_attachments;
    ClearValueList m_clear_values;
    std::optional<uint32_t> m_depth_stencil_attachment_index;
};


} // namespace lcf::vkc
