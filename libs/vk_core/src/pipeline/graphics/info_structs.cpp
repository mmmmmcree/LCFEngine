#include "vk_core/pipeline/graphics/info_structs.h"
#include <cassert>
#include <atomic>
#include <ranges>

namespace stdr = std::ranges;
namespace stdv = std::views;

namespace {

uint32_t next_attachment_set_id() noexcept;

} // anonymous namespace

namespace lcf::vkc {

uint32_t AttachmentSetInfo::getIndex(ColorAttachmentKey key) const noexcept
{
    assert(key.validate(m_set_id) and "color attachment key from a different attachment set");
    return key.getIndex();
}

uint32_t AttachmentSetInfo::getIndex(ResolveAttachmentKey key) const noexcept
{
    assert(key.validate(m_set_id) and "resolve attachment key from a different attachment set");
    return m_color_resolve_list[key.getIndex()].second;
}

uint32_t AttachmentSetInfo::getIndex(DepthStencilAttachmentKey key) const noexcept
{
    assert(key.validate(m_set_id) and "depth stencil attachment key from a different attachment set");
    return static_cast<uint32_t>(m_resource_info_list.size()) - 1u;
}

AttachmentResourceInfo * AttachmentSetInfo::getResolveAttachmentResource(ColorAttachmentKey key) noexcept
{
    auto && [resolve_mode, index] = m_color_resolve_list[key.getIndex()];
    if (resolve_mode == vk::ResolveModeFlagBits::eNone) { return nullptr; }
    return &m_resource_info_list[index];
}

AttachmentResourceInfo * AttachmentSetInfo::getDepthStencilAttachmentResource() noexcept
{
    return m_has_depth_stencil ? &m_resource_info_list.back() : nullptr;
}

std::span<AttachmentResourceInfo> AttachmentSetInfo::viewColorAttachmentResources() noexcept
{
    return std::span(m_resource_info_list).subspan(0, this->getColorAttachmentCount());
}

AttachmentSetInfoBuilder::AttachmentSetInfoBuilder() noexcept :
    m_set_id(next_attachment_set_id())
{
}

ResolveAttachmentKey AttachmentSetInfoBuilder::enableResolveAttachment(ColorAttachmentKey source, vk::ResolveModeFlagBits mode) noexcept
{
    assert(source.validate(m_set_id) and "color key from a different builder");
    assert(mode != vk::ResolveModeFlagBits::eNone and "resolve mode must not be none; a key always denotes an existing resolve attachment");
    m_color_resolve_modes[source.getIndex()] = mode;
    return ResolveAttachmentKey { m_set_id, source.getIndex() };
}


AttachmentSetInfo AttachmentSetInfoBuilder::build() const noexcept
{
    uint32_t color_count = static_cast<uint32_t>(m_color_resource_info_list.size());
    uint32_t resolved_color_count = static_cast<uint32_t>(m_color_resolve_modes.size());
    ResourceInfoList resource_info_list;
    resource_info_list.reserve(color_count + resolved_color_count + m_has_depth_stencil);
    resource_info_list.append_range(std::exchange(m_color_resource_info_list, {}));
    AttachmentSetInfo::ColorResolveList color_resolve_list(color_count, {vk::ResolveModeFlagBits::eNone, vk::AttachmentUnused});
    for (auto && [color_index, resolve_mode] : m_color_resolve_modes) {
        color_resolve_list[color_index] = {resolve_mode, static_cast<uint32_t>(resource_info_list.size())};
        resource_info_list.emplace_back().setSampleCount(vk::SampleCountFlagBits::e1);
    }
    if (m_has_depth_stencil) { resource_info_list.emplace_back(); }
    m_color_resolve_modes.clear();
    return AttachmentSetInfo {
        std::move(resource_info_list),
        std::move(color_resolve_list),
        std::exchange(m_set_id, next_attachment_set_id()),
        std::exchange(m_has_depth_stencil, false)
    };
}

auto RenderTargetInfo::setSampleCount(vk::SampleCountFlagBits sample_count) noexcept -> Self &
{
    m_sample_count = std::max(sample_count, m_min_sample_count);
    for (auto & resource_info : m_set.viewColorAttachmentResources()) {
        resource_info.setSampleCount(m_sample_count);
    }
    if (auto depth_stencil_resource_p = m_set.getDepthStencilAttachmentResource()) {
        depth_stencil_resource_p->setSampleCount(m_sample_count);
    }
    return *this;
}


StaticRenderInfo::StaticRenderInfo(AttachmentSetInfo & attachments, bool unified_layouts_enabled) noexcept :
    m_attachments(attachments),
    m_pass_infos(attachments.getAttachmentCount()),
    m_unified_layout_enabled(unified_layouts_enabled)
{
    vk::ImageLayout color_layout = enum_traits<AttachmentUsage>::layout_of(AttachmentUsage::eColorAttachment, m_unified_layout_enabled);
    for (uint32_t i = 0; i < m_attachments.getAttachmentCount(); ++i) {
        m_pass_infos[i].setFinalLayout(color_layout);
    }
    if (m_attachments.hasDepthStencilAttachment()) {
        vk::ImageLayout depth_stencil_layout = enum_traits<AttachmentUsage>::layout_of(AttachmentUsage::eDepthStencilAttachment, m_unified_layout_enabled);
        m_pass_infos.back().setFinalLayout(depth_stencil_layout);
    }
}

std::vector<vk::AttachmentDescription2> StaticRenderInfo::makeAttachmentDescriptions() const noexcept
{
    auto attachment_resources = m_attachments.viewAttachmentResources();
    std::vector<vk::AttachmentDescription2> descriptions;
    descriptions.reserve(m_pass_infos.size());
    for (const auto & [resource, pass_info] : stdv::zip(attachment_resources, m_pass_infos)) {
        vk::AttachmentDescription2 & description = descriptions.emplace_back(pass_info);
        description.setFormat(resource.getFormat()).setSamples(resource.getSampleCount());
        if (utils::discards_on_load(resource.getFormat(), pass_info.getLoadOp(), pass_info.getStencilLoadOp())) {
            description.setInitialLayout(vk::ImageLayout::eUndefined);
        }
    }
    return descriptions;
}

std::optional<vk::ImageMemoryBarrier2> DynamicRenderInfo::AttachmentPassInfo::makeEntryBarrier(bool is_load_discards) const noexcept
{
    vk::ImageLayout in_pass_layout = m_in_pass_attributes.getImageLayout();
    vk::ImageLayout old_layout = is_load_discards ? vk::ImageLayout::eUndefined : m_entry_attributes.getImageLayout();
    vk::PipelineStageFlags2 src_stage = m_entry_attributes.getStageFlags();
    if (old_layout == in_pass_layout and not src_stage) { return std::nullopt; }
    vk::ImageMemoryBarrier2 barrier;
    barrier.setOldLayout(old_layout)
        .setNewLayout(in_pass_layout)
        .setSrcStageMask(src_stage)
        .setSrcAccessMask(is_load_discards ? vk::AccessFlags2 {} : m_entry_attributes.getAccessFlags())
        .setDstStageMask(m_in_pass_attributes.getStageFlags())
        .setDstAccessMask(m_in_pass_attributes.getAccessFlags())
        .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
        .setDstQueueFamilyIndex(vk::QueueFamilyIgnored);
    return barrier;
}

std::optional<vk::ImageMemoryBarrier2> DynamicRenderInfo::AttachmentPassInfo::makeExitBarrier(bool is_store_discards) const noexcept
{
    vk::ImageLayout exit_layout = m_exit_attributes.getImageLayout();
    if (exit_layout == vk::ImageLayout::eUndefined) { return std::nullopt; }
    vk::ImageMemoryBarrier2 barrier;
    barrier.setOldLayout(is_store_discards ? vk::ImageLayout::eUndefined : m_in_pass_attributes.getImageLayout())
        .setNewLayout(exit_layout)
        .setSrcStageMask(m_in_pass_attributes.getStageFlags())
        .setSrcAccessMask(m_in_pass_attributes.getAccessFlags())
        .setDstStageMask(m_exit_attributes.getStageFlags())
        .setDstAccessMask(m_exit_attributes.getAccessFlags())
        .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
        .setDstQueueFamilyIndex(vk::QueueFamilyIgnored);
    return barrier;
}

DynamicRenderInfo::DynamicRenderInfo(AttachmentSetInfo & attachments, bool unified_layouts_enabled) noexcept :
    m_attachments(attachments),
    m_pass_infos(attachments.getAttachmentCount()),
    m_unified_layout_enabled(unified_layouts_enabled)
{
    auto init_pass_info = [](AttachmentPassInfo & pass_info, const AttachmentUsageAttributes & in_pass) noexcept {
        pass_info.m_in_pass_attributes = in_pass;
        pass_info.m_entry_attributes = AttachmentUsageAttributes {
            in_pass.getImageLayout(),
            vk::PipelineStageFlagBits2::eAllCommands,
            vk::AccessFlagBits2::eMemoryWrite};
    };
    AttachmentUsageAttributes color_attributes {AttachmentUsage::eColorAttachment, m_unified_layout_enabled};
    for (auto & pass_info : m_pass_infos) { init_pass_info(pass_info, color_attributes); }
    if (m_attachments.hasDepthStencilAttachment()) {
        init_pass_info(m_pass_infos.back(), AttachmentUsageAttributes {AttachmentUsage::eDepthStencilAttachment, m_unified_layout_enabled});
    }
}

} // namespace lcf::vkc

namespace {

uint32_t next_attachment_set_id() noexcept
{
    static std::atomic<uint32_t> counter {0u};
    return counter.fetch_add(1u, std::memory_order_relaxed);
}

} // anonymous namespace