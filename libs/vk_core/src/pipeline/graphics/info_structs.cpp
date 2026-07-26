#include "vk_core/pipeline/graphics/info_structs.h"
#include <cassert>
#include <atomic>

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
    return static_cast<uint32_t>(m_descriptions.size()) - 1u;
}

AttachmentDescriptionInfo * AttachmentSetInfo::getResolveAttachmentDescription(ColorAttachmentKey key) noexcept
{
    auto && [resolve_mode, index] = m_color_resolve_list[key.getIndex()];
    if (resolve_mode == vk::ResolveModeFlagBits::eNone) { return nullptr; }
    return &m_descriptions[index];
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
    uint32_t color_count = static_cast<uint32_t>(m_color_descriptions.size());
    uint32_t resolved_color_count = static_cast<uint32_t>(m_color_resolve_modes.size());
    DescriptionList descriptions;
    descriptions.reserve(color_count + resolved_color_count + m_has_depth_stencil);
    descriptions.append_range(std::exchange(m_color_descriptions, {}));
    std::vector<std::pair<vk::ResolveModeFlagBits, uint32_t>> color_resolve_list(color_count, {vk::ResolveModeFlagBits::eNone, vk::AttachmentUnused});
    for (auto && [color_index, resolve_mode] : m_color_resolve_modes) {
        color_resolve_list[color_index] = {resolve_mode, static_cast<uint32_t>(descriptions.size())};
        descriptions.emplace_back().setSampleCount(vk::SampleCountFlagBits::e1);
    }
    if (m_has_depth_stencil) { descriptions.emplace_back(); }
    m_color_resolve_modes.clear();
    return AttachmentSetInfo {
        std::move(descriptions),
        std::move(color_resolve_list),
        std::exchange(m_set_id, next_attachment_set_id()),
        std::exchange(m_has_depth_stencil, false)
    };
}

auto RenderTargetInfo::setSampleCount(vk::SampleCountFlagBits sample_count) noexcept -> Self &
{
    m_sample_count = std::max(sample_count, m_min_sample_count);
    for (auto & desc : m_set.viewColorAttachmentDescriptions()) { desc.setSampleCount(m_sample_count); }
    if (auto depth_stencil_desc_p = m_set.getDepthStencilAttachmentDescription()) {
        depth_stencil_desc_p->setSampleCount(m_sample_count);
    }
    return *this;
}

} // namespace lcf::vkc

namespace {

uint32_t next_attachment_set_id() noexcept
{
    static std::atomic<uint32_t> counter {0u};
    return counter.fetch_add(1u, std::memory_order_relaxed);
}

} // anonymous namespace