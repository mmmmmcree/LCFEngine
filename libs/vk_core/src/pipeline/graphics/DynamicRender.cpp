#include "vk_core/pipeline/graphics/DynamicRender.h"
#include "vk_core/pipeline/graphics/entry.h"
#include "vk_core/manifest/DeviceExtensionManifest.h"
#include "vk_core/pipeline/graphics/RenderTarget.h"
#include "vk_core/pipeline/graphics/info_structs.h"
#include "vk_core/command/CommandBufferProxy.h"
#include "vk_core/utils/format_utils.h"
#include <ranges>

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

std::error_code DynamicRender::create(const DynamicRenderInfo & render_info) noexcept
{
    m_rendering = render_info.m_rendering;
    auto pass_infos = render_info.viewPassInfos();
    auto resources = render_info.viewAttachmentResources();
    const auto & color_resolve_list = render_info.getColorResolveList();
    uint32_t color_count = render_info.getColorAttachmentCount();
    uint32_t attachment_count = render_info.getAttachmentCount();
    m_color_attachments.clear(); m_color_attachments.reserve(color_count);
    m_color_formats.clear(); m_color_formats.reserve(color_count);
    m_resolve_indices.clear(); m_resolve_indices.reserve(color_count);
    m_barrier_slots.clear(); m_barrier_slots.reserve(attachment_count);
    m_entry_barriers.clear();
    m_exit_barriers.clear();
    m_depth_format = m_stencil_format = vk::Format::eUndefined;

    for (uint32_t i = 0; i < color_count; ++i) {
        const auto & pass_info = pass_infos[i];
        auto [resolve_mode, resolve_index] = color_resolve_list[i];
        m_color_attachments.emplace_back()
            .setImageLayout(pass_info.m_in_pass_attributes.getImageLayout())
            .setResolveMode(resolve_mode)
            .setLoadOp(pass_info.m_load_op)
            .setStoreOp(pass_info.m_store_op);
        if (resolve_index != vk::AttachmentUnused) {
            m_color_attachments.back().setResolveImageLayout(pass_infos[resolve_index].m_in_pass_attributes.getImageLayout());
        }
        m_resolve_indices.emplace_back(resolve_index);
        m_color_formats.emplace_back(resources[i].getFormat());
    }
    if (render_info.hasDepthStencilAttachment()) {
        const auto & pass_info = pass_infos.back();
        vk::Format format = resources.back().getFormat();
        vk::RenderingAttachmentInfo depth_stencil_attachment;
        depth_stencil_attachment.setImageLayout(pass_info.m_in_pass_attributes.getImageLayout())
            .setLoadOp(pass_info.m_load_op)
            .setStoreOp(pass_info.m_store_op);
        if (utils::is_depth_format(format)) {
            m_depth_attachment = depth_stencil_attachment;
            m_depth_format = format;
        }
        if (utils::is_stencil_format(format)) {
            m_stencil_attachment = depth_stencil_attachment
                .setLoadOp(pass_info.m_stencil_load_op)
                .setStoreOp(pass_info.m_stencil_store_op);
            m_stencil_format = format;
        }
    }
    for (uint32_t i = 0; i < attachment_count; ++i) {
        const auto & pass_info = pass_infos[i];
        vk::Format format = resources[i].getFormat();
        bool load_discards = utils::discards_on_load(format, pass_info.m_load_op, pass_info.m_stencil_load_op);
        bool store_discards = utils::discards_on_store(format, pass_info.m_store_op, pass_info.m_stencil_store_op);
        auto entry_barrier = pass_info.makeEntryBarrier(load_discards);
        auto exit_barrier = pass_info.makeExitBarrier(store_discards);
        if (not entry_barrier and not exit_barrier) { continue; }
        BarrierSlot & barrier_slot = m_barrier_slots.emplace_back();
        barrier_slot.slot_index = i;
        if (entry_barrier) {
            barrier_slot.entry_index = static_cast<uint32_t>(m_entry_barriers.size());
            m_entry_barriers.emplace_back(*entry_barrier);
        }
        if (exit_barrier) {
            barrier_slot.exit_index = static_cast<uint32_t>(m_exit_barriers.size());
            m_exit_barriers.emplace_back(*exit_barrier);
        }
    }
    return {};
}

void DynamicRender::begin(CommandBufferProxy &cmd, const RenderTarget &render_target) noexcept
{
    auto attachments = render_target.viewAttachments();
    for (const auto & [slot_index, entry_index, exit_index] : m_barrier_slots) {
        const Attachment & attachment = attachments[slot_index];
        vk::Image image = attachment.getImage();
        vk::ImageSubresourceRange range = attachment.getDescription().getSubresourceRange();
        if (entry_index != vk::AttachmentUnused) {
            m_entry_barriers[entry_index].setImage(image).setSubresourceRange(range);
        }
        if (exit_index != vk::AttachmentUnused) {
            m_exit_barriers[exit_index].setImage(image).setSubresourceRange(range);
        }
    }
    if (not m_entry_barriers.empty()) {
        vk::DependencyInfo dependency_info;
        dependency_info.setImageMemoryBarriers(m_entry_barriers);
        cmd.pipelineBarrier2(dependency_info);

    }
    auto image_views = render_target.viewAttachmentImageViews();
    const auto & clear_values = render_target.getClearValues();
    for (uint32_t color_index = 0; color_index < m_color_attachments.size(); ++color_index) {
        auto & color_attachment = m_color_attachments[color_index];
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
    if (m_exit_barriers.empty()) { return; }
    vk::DependencyInfo dependency_info;
    dependency_info.setImageMemoryBarriers(m_exit_barriers);
    cmd.pipelineBarrier2(dependency_info);
}

} // namespace lcf::vkc