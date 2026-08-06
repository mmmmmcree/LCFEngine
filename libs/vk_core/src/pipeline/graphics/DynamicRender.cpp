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

namespace {
using namespace lcf::vkc;

struct ResolvedTransition
{
    vk::ImageLayout entry_layout;
    vk::ImageLayout in_pass_layout;
    vk::ImageLayout exit_layout;      //- eUndefined when exit_usage is eNone
    vk::ImageLayout exit_old_layout;  //- in_pass_layout, or eUndefined when storeOp discards
    AccessScope entry_src_scope;
    AccessScope in_pass_scope;
    AccessScope exit_dst_scope;
    vk::ImageUsageFlags required_image_usage; //- union of the three intents
    bool entry_barrier_needed;
    bool exit_barrier_needed;
};

ResolvedTransition resolve_transition(
    const AttachmentDescriptionInfo & description,
    const AttachmentTransitionInfo & transition,
    bool unified_enabled) noexcept;

using BarrierSlotMap = typename DynamicRender::BarrierSlotMap;
using BarrierList = typename DynamicRender::BarrierList;
using BarrierSlotMapList = typename DynamicRender::BarrierSlotMapList;
struct BakeBarriersResult
{
    BarrierList entry_barriers;
    BarrierList exit_barriers;
    BarrierSlotMapList barrier_slot_maps;
};

BakeBarriersResult bake_barriers(std::span<const ResolvedTransition> resolved) noexcept;

} // anonymous namespace

namespace lcf::vkc {

std::error_code DynamicRender::create(const DynamicRenderInfo & render_info) noexcept
{
    m_rendering = render_info.m_rendering;
    const auto & descriptions = render_info.getAttachmentDescriptions();
    const auto & color_resolve_list = render_info.getColorResolveList();
    const auto & transitions = render_info.getTransitions();
    bool unified_enabled = render_info.isUnifiedLayoutEnabled();
    uint32_t color_count = render_info.getColorAttachmentCount();
    uint32_t attachment_count = static_cast<uint32_t>(descriptions.size());
    m_color_attachments.clear();
    m_color_formats.clear();
    m_resolve_indices.clear();
    m_entry_barriers.clear();
    m_exit_barriers.clear();
    m_barrier_slot_maps.clear();
    m_color_attachments.reserve(color_count);
    m_color_formats.reserve(color_count);
    m_resolve_indices.reserve(color_count);
    m_barrier_slot_maps.reserve(attachment_count);
    m_dependency_flags = vk::DependencyFlags {};

    //- one resolve per slot, reused by both the rendering info and the barriers
    std::vector<ResolvedTransition> resolved;
    resolved.reserve(attachment_count);
    for (uint32_t slot = 0; slot < attachment_count; ++slot) {
        resolved.emplace_back(resolve_transition(descriptions[slot], transitions[slot], unified_enabled));
    }

    for (uint32_t color_index = 0; color_index < color_count; ++color_index) {
        const AttachmentDescriptionInfo & description = descriptions[color_index];
        auto [resolve_mode, resolve_index] = color_resolve_list[color_index];
        m_color_attachments.emplace_back()
            .setImageLayout(resolved[color_index].in_pass_layout)
            .setResolveMode(resolve_mode)
            .setLoadOp(description.getLoadOp())
            .setStoreOp(description.getStoreOp());
        if (resolve_index != vk::AttachmentUnused) {
            m_color_attachments.back().setResolveImageLayout(resolved[resolve_index].in_pass_layout);
        }
        m_resolve_indices.emplace_back(resolve_index);
        m_color_formats.emplace_back(description.getFormat());
    }
    if (render_info.hasDepthStencilAttachment()) {
        const AttachmentDescriptionInfo & description = descriptions.back();
        vk::RenderingAttachmentInfo depth_stencil_attachment;
        depth_stencil_attachment.setImageLayout(resolved.back().in_pass_layout)
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
    }
    auto bake_result = bake_barriers(resolved);
    m_entry_barriers = std::move(bake_result.entry_barriers);
    m_exit_barriers = std::move(bake_result.exit_barriers);
    m_barrier_slot_maps = std::move(bake_result.barrier_slot_maps);
    return {};
}

void DynamicRender::begin(CommandBufferProxy &cmd, const RenderTarget &render_target) noexcept
{
    auto attachments = render_target.viewAttachments();
    for (const auto & slot_map : m_barrier_slot_maps) {
        const Attachment & attachment = attachments[slot_map.slot];
        vk::Image image = attachment.getImage();
        vk::ImageSubresourceRange range = attachment.getDescription().getSubresourceRange();
        if (slot_map.entry_index != vk::AttachmentUnused) {
            m_entry_barriers[slot_map.entry_index].setImage(image).setSubresourceRange(range);
        }
        if (slot_map.exit_index != vk::AttachmentUnused) {
            m_exit_barriers[slot_map.exit_index].setImage(image).setSubresourceRange(range);
        }
    }
    if (not m_entry_barriers.empty()) {
        vk::DependencyInfo dependency_info;
        dependency_info.setDependencyFlags(m_dependency_flags)
            .setImageMemoryBarriers(m_entry_barriers);
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

namespace {

bool discards_on_load(const AttachmentDescriptionInfo & description) noexcept
{
    constexpr auto is_discard = [](vk::AttachmentLoadOp op) noexcept {
        return op == vk::AttachmentLoadOp::eClear or op == vk::AttachmentLoadOp::eDontCare;
    };
    vk::Format format = description.getFormat();
    //- a depth-stencil slot only downgrades when every aspect it actually has is discarded;
    //- Vulkan ignores stencilLoadOp on a depth-only format, so don't let its default veto
    bool depth_discarded = not utils::is_depth_format(format) or is_discard(description.getLoadOp());
    bool stencil_discarded = not utils::is_stencil_format(format) or is_discard(description.getStencilLoadOp());
    if (utils::is_depth_format(format) or utils::is_stencil_format(format)) {
        return depth_discarded and stencil_discarded;
    }
    return is_discard(description.getLoadOp());
}

bool discards_on_store(const AttachmentDescriptionInfo & description) noexcept
{
    //- eNone is not a discard: it leaves contents either preserved or undefined, and treating
    //- "maybe preserved" as discardable would throw away data the caller may still want
    constexpr auto is_discard = [](vk::AttachmentStoreOp op) noexcept {
        return op == vk::AttachmentStoreOp::eDontCare;
    };
    vk::Format format = description.getFormat();
    bool depth_discarded = not utils::is_depth_format(format) or is_discard(description.getStoreOp());
    bool stencil_discarded = not utils::is_stencil_format(format) or is_discard(description.getStencilStoreOp());
    if (utils::is_depth_format(format) or utils::is_stencil_format(format)) {
        return depth_discarded and stencil_discarded;
    }
    return is_discard(description.getStoreOp());
}

ResolvedTransition resolve_transition(
    const AttachmentDescriptionInfo & description,
    const AttachmentTransitionInfo & transition,
    bool unified_enabled) noexcept
{
    using AttachmentUsageTraits = lcf::enum_specialized_attributes_traits<AttachmentUsage>;
    auto to_image_layout = [](AttachmentUsage usage, bool unified_enabled) noexcept {
        return AttachmentUsageTraits::layout_of(usage, unified_enabled);
    };
    auto to_access_scope = [](AttachmentUsage usage) noexcept {
        return AccessScope{ AttachmentUsageTraits::stage_mask_of(usage), AttachmentUsageTraits::access_mask_of(usage) };
    };
    auto to_required_image_usage = [](AttachmentUsage usage) noexcept {
        return AttachmentUsageTraits::required_image_usage_of(usage);
    };
    AttachmentUsage in_pass_usage = transition.getInPassUsage();
    AttachmentUsage entry_usage = transition.getEntryUsage();
    if (entry_usage == AttachmentUsage::eNone) {
        //- undeclared: derive from loadOp
        entry_usage = discards_on_load(description) ? AttachmentUsage::eDiscard : in_pass_usage;
    }
    AttachmentUsage exit_usage = transition.getExitUsage();

    ResolvedTransition resolved;
    resolved.in_pass_layout = transition.getInPassLayoutOr(to_image_layout(in_pass_usage, unified_enabled));
    resolved.in_pass_scope = to_access_scope(in_pass_usage);
    resolved.entry_layout = to_image_layout(entry_usage, unified_enabled);
    resolved.entry_src_scope = transition.getEntrySrcScopeOr(to_access_scope(entry_usage));
    resolved.exit_layout = to_image_layout(exit_usage, unified_enabled);
    resolved.exit_dst_scope = transition.getExitDstScopeOr(to_access_scope(exit_usage));
    resolved.exit_old_layout = discards_on_store(description) ? vk::ImageLayout::eUndefined : resolved.in_pass_layout;
    resolved.required_image_usage = to_required_image_usage(entry_usage) |
        to_required_image_usage(in_pass_usage) |
        to_required_image_usage(exit_usage);
    resolved.entry_barrier_needed = resolved.entry_layout != resolved.in_pass_layout or
        static_cast<bool>(resolved.entry_src_scope.m_stage_mask);
    resolved.exit_barrier_needed = exit_usage != AttachmentUsage::eNone;
    return resolved;
}

BakeBarriersResult bake_barriers(std::span<const ResolvedTransition> resolved) noexcept
{
    BakeBarriersResult result;
    for (std::size_t slot = 0; slot < resolved.size(); ++slot) {
        const auto & transition = resolved[slot];
        BarrierSlotMap slot_map;
        slot_map.slot = static_cast<uint32_t>(slot);
        slot_map.required_image_usage = transition.required_image_usage;
        if (transition.entry_barrier_needed) {
            slot_map.entry_index = static_cast<uint32_t>(result.entry_barriers.size());
            result.entry_barriers.emplace_back()
                .setOldLayout(transition.entry_layout)
                .setNewLayout(transition.in_pass_layout)
                .setSrcStageMask(transition.entry_src_scope.m_stage_mask)
                .setSrcAccessMask(transition.entry_src_scope.m_access_mask)
                .setDstStageMask(transition.in_pass_scope.m_stage_mask)
                .setDstAccessMask(transition.in_pass_scope.m_access_mask);
        }
        if (transition.exit_barrier_needed) {
            slot_map.exit_index = static_cast<uint32_t>(result.exit_barriers.size());
            result.exit_barriers.emplace_back()
                .setOldLayout(transition.exit_old_layout)
                .setNewLayout(transition.exit_layout)
                .setSrcStageMask(transition.in_pass_scope.m_stage_mask)
                .setSrcAccessMask(transition.in_pass_scope.m_access_mask)
                .setDstStageMask(transition.exit_dst_scope.m_stage_mask)
                .setDstAccessMask(transition.exit_dst_scope.m_access_mask);
        }
        if (slot_map.entry_index != vk::AttachmentUnused or slot_map.exit_index != vk::AttachmentUnused) {
            result.barrier_slot_maps.emplace_back(slot_map);
        }
    }
    return result;
}

} // anonymous namespace