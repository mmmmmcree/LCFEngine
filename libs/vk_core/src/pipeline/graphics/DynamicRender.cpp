#include "vk_core/pipeline/graphics/DynamicRender.h"
#include "vk_core/pipeline/graphics/entry.h"
#include "vk_core/manifest/DeviceExtensionManifest.h"
#include "vk_core/pipeline/graphics/RenderTarget.h"
#include "vk_core/pipeline/graphics/info_structs.h"
#include "vk_core/command/CommandBufferProxy.h"
#include "vk_core/utils/format_utils.h"
#include <ranges>

namespace stdr = std::ranges;
namespace stdv = std::views;

namespace lcf::vkc::entry {

void register_dynamic_render(DeviceExtensionManifest & manifest) noexcept
{
    static constexpr std::array k_features
    {
        LCF_VKC_UTILS_FEATURE_BIT(&vk::PhysicalDeviceVulkan13Features::dynamicRendering),
    };
    manifest.addRequiredFeatures(k_features);
}

} // namespace lcf::vkc::entry

namespace lcf::vkc {

std::error_code DynamicRender::create(const DynamicRenderInfo &render_info) noexcept
{
    m_rendering = render_info.m_rendering;
    const auto & descriptions = render_info.getAttachmentDescriptions();
    const auto & color_resolve_list = render_info.getColorResolveList();
    const auto & layouts = render_info.getLayouts();
    uint32_t color_count = render_info.getColorAttachmentCount();
    m_color_attachments.clear();
    m_color_formats.clear();
    m_resolve_indices.clear();
    m_color_attachments.reserve(color_count);
    m_color_formats.reserve(color_count);
    m_resolve_indices.reserve(color_count);
    for (uint32_t color_index = 0; color_index < color_count; ++color_index) {
        const AttachmentDescriptionInfo & description = descriptions[color_index];
        auto [resolve_mode, resolve_index] = color_resolve_list[color_index];
        m_color_attachments.emplace_back()
            .setImageLayout(layouts[color_index])
            .setResolveMode(resolve_mode)
            .setLoadOp(description.getLoadOp())
            .setStoreOp(description.getStoreOp());
        if (resolve_index != vk::AttachmentUnused) {
            m_color_attachments.back().setResolveImageLayout(layouts[resolve_index]);
        }
        m_resolve_indices.emplace_back(resolve_index);
        m_color_formats.emplace_back(description.getFormat());
    }
    if (not render_info.hasDepthStencilAttachment()) { return {}; }
    const AttachmentDescriptionInfo & description = descriptions.back();
    vk::RenderingAttachmentInfo depth_stencil_attachment;
    depth_stencil_attachment.setImageLayout(layouts.back())
        .setLoadOp(description.getLoadOp())
        .setStoreOp(description.getStoreOp());
    if (utils::is_depth_format(description.getFormat())) {
        m_depth_attachment = depth_stencil_attachment;
        m_depth_format = description.getFormat();
    }
    if (utils::is_stencil_format(description.getFormat())) {
        m_stencil_attachment = depth_stencil_attachment;
        m_stencil_format = description.getFormat();
    }
    return {};
}

void DynamicRender::begin(CommandBufferProxy &cmd, const RenderTarget &render_target) noexcept
{
    auto image_views = render_target.viewAttachmentImageViews();
    const auto & clear_values = render_target.getClearValues();
    for (auto && [color_index, color_attachment] : m_color_attachments | stdv::enumerate) {
        color_attachment.setImageView(image_views[color_index])
            .setClearValue(clear_values[color_index]);
        uint32_t resolve_slot = m_resolve_indices[color_index];
        if (resolve_slot != vk::AttachmentUnused) {
            color_attachment.setResolveImageView(image_views[resolve_slot]);
        }
    }
    vk::RenderingInfo rendering_info = m_rendering.root();
    rendering_info.setRenderArea(render_target.getRenderArea())
        .setLayerCount(render_target.getLayerCount())
        .setColorAttachments(m_color_attachments);
    if (m_depth_format != vk::Format::eUndefined) {
        m_depth_attachment.setImageView(image_views.back())
            .setClearValue(clear_values.back());
        rendering_info.setPDepthAttachment(&m_depth_attachment);
    }
    if (m_stencil_format!= vk::Format::eUndefined) {
        m_stencil_attachment.setImageView(image_views.back())
            .setClearValue(clear_values.back());
        rendering_info.setPStencilAttachment(&m_stencil_attachment);
    }
    cmd.beginRendering(rendering_info);
}

void DynamicRender::end(CommandBufferProxy &cmd) noexcept
{
    cmd.endRendering();
}

} // namespace lcf::vkc