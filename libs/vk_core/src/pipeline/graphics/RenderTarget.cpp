#include "vk_core/pipeline/graphics/RenderTarget.h"
#include "vk_core/pipeline/graphics/info_structs.h"

namespace lcf::vkc {

std::error_code RenderTarget::build(const RenderTargetInfo & info) noexcept
{
    m_max_extent = info.getExtent();
    m_render_area = vk::Rect2D { {0, 0}, m_max_extent };
    m_attachments.resize(info.getColorFormats().size());
    // one clear value per color attachment (default black); canonical [color..][depth] order
    m_clear_values.assign(info.getColorFormats().size(),
        vk::ClearValue{}.setColor(vk::ClearColorValue{std::array<float, 4>{1.0f, 1.0f, 1.0f, 1.0f}}));
    return {};
}

std::error_code RenderTarget::setColorAttachment(uint32_t index, const Image &image, uint32_t mip_level, uint32_t array_layer) noexcept
{
    if (index >= m_attachments.size()) { return std::make_error_code(std::errc::argument_out_of_domain); }
    AttachmentDescription attachment_desc;
    attachment_desc.setArrayLayerCount(m_layer_count)
        .setBaseMipLevel(mip_level)
        .setBaseArrayLayer(array_layer)
        .addAspectFlags(vk::ImageAspectFlagBits::eColor);
    Attachment attachment;
    if (auto ec = attachment.create(image, attachment_desc)) { return ec; }
    m_attachments[index] = std::move(attachment);
    return std::error_code();
}
} // namespace lcf::vkc

