#include "vk_core/pipeline/graphics/RenderTarget.h"
#include "vk_core/pipeline/graphics/info_structs.h"
#include <cassert>
#include <ranges>

namespace stdr = std::ranges;
namespace stdv = std::views;

namespace lcf::vkc {

std::error_code RenderTarget::build(const RenderTargetInfo &info) noexcept
{
    m_set_id = info.getSetId();
    m_resolve_attachment_index_map = info.getColorResolveList() |
        stdv::transform(&std::pair<vk::ResolveModeFlagBits, uint32_t>::second) |
        stdr::to<std::vector>();
    uint32_t attachment_count = info.getAttachmentCount();
    m_attachments.clear();
    m_attachments.resize(attachment_count);
    m_clear_values.assign(attachment_count, vk::ClearValue{}.setColor(vk::ClearColorValue{std::array<float, 4>{1.0f, 1.0f, 1.0f, 1.0f}}));
    if (info.hasDepthStencilAttachment()) { m_clear_values.back().setDepthStencil({1.0f, 0u}); }
    m_max_extent = info.getExtent();
    m_render_area = vk::Rect2D { {0, 0}, m_max_extent };
    return {};
}

std::error_code RenderTarget::setColorAttachment(const ColorAttachmentKey &key, const Image &image, uint32_t mip_level, uint32_t array_layer) noexcept
{
    AttachmentDescription attachment_desc;
    attachment_desc.setArrayLayerCount(m_layer_count)
        .setBaseMipLevel(mip_level)
        .setBaseArrayLayer(array_layer)
        .addAspectFlags(vk::ImageAspectFlagBits::eColor);
    Attachment attachment;
    if (auto ec = attachment.create(image, attachment_desc)) { return ec; }
    m_attachments[this->getIndex(key)] = std::move(attachment);
    return {};
}

uint32_t RenderTarget::getIndex(const ColorAttachmentKey &key) const noexcept
{
    assert(key.validate(m_set_id) and "color attachment key from a different attachment set");
    return key.getIndex();
}

uint32_t RenderTarget::getIndex(const DepthStencilAttachmentKey &key) const noexcept
{
    assert(key.validate(m_set_id) and "depth stencil attachment key from a different attachment set");
    return static_cast<uint32_t>(m_attachments.size() - 1u);
}

uint32_t RenderTarget::getIndex(const ResolveAttachmentKey &key) const noexcept
{
    assert(key.validate(m_set_id) and "resolve attachment key from a different attachment set");
    return m_resolve_attachment_index_map[key.getIndex()];
}

} // namespace lcf::vkc
