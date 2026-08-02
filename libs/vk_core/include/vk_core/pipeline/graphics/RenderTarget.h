#pragma once

#include "vk_core/memory/Image.h"
#include <vulkan/vulkan.hpp>
#include <vector>
#include <ranges>

namespace lcf::vkc {

class ColorAttachmentKey;
class DepthStencilAttachmentKey;
class ResolveAttachmentKey;
class RenderTargetInfo;

class RenderTarget
{
    using Self = RenderTarget;
    using AttachmentList = std::vector<Attachment>;
    using ClearValueList = std::vector<vk::ClearValue>;
    using ResolveAttachmentIndexMap = std::vector<uint32_t>;
public:
    std::error_code build(const RenderTargetInfo & info) noexcept;
    std::error_code setColorAttachment(const ColorAttachmentKey & key, const Image & image, uint32_t mip_level = 0, uint32_t array_layer = 0) noexcept;
    Self & setRenderArea(const vk::Rect2D & render_area) noexcept { m_render_area = render_area; return *this; }
    const vk::Extent2D & getMaxExtent() const noexcept { return m_max_extent; }
    const vk::Rect2D & getRenderArea() const noexcept { return m_render_area; }
    const uint32_t & getLayerCount() const noexcept { return m_layer_count; }
    const Attachment & getAttachment(const ColorAttachmentKey & key) const noexcept { return m_attachments[this->getIndex(key)]; }
    const Attachment & getAttachment(const DepthStencilAttachmentKey & key) const noexcept { return m_attachments[this->getIndex(key)]; }
    const Attachment & getAttachment(const ResolveAttachmentKey & key) const noexcept { return m_attachments[this->getIndex(key)]; }
    std::span<const Attachment> viewAttachments() const noexcept { return m_attachments; }
    auto viewAttachmentImageViews() const noexcept { return m_attachments | std::views::transform(&Attachment::getImageView); }
    const ClearValueList & getClearValues() const noexcept { return m_clear_values; }
private:
    uint32_t getIndex(const ColorAttachmentKey & key) const noexcept;
    uint32_t getIndex(const DepthStencilAttachmentKey & key) const noexcept;
    uint32_t getIndex(const ResolveAttachmentKey & key) const noexcept;
private:
    uint32_t m_set_id = -1u;
    ResolveAttachmentIndexMap m_resolve_attachment_index_map;
    AttachmentList m_attachments;
    ClearValueList m_clear_values;
    vk::Extent2D m_max_extent;
    vk::Rect2D m_render_area;
    uint32_t m_layer_count = 1;
};


} // namespace lcf::vkc
