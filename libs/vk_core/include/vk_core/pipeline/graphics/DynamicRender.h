#pragma once

#include <vulkan/vulkan.hpp>
#include <vector>
#include <optional>
#include <span>
#include <system_error>
#include "vk_core/utils/DynamicStructureChain.h"

namespace lcf::vkc {

class DynamicRenderInfo;
class RenderTarget;
class CommandBufferProxy;

class DynamicRenderScopeInfo
{
    using FormatListView = std::span<const vk::Format>;
public:
    ~DynamicRenderScopeInfo() noexcept = default;
    DynamicRenderScopeInfo(
        FormatListView color_formats,
        vk::Format depth_format,
        vk::Format stencil_format,
        uint32_t view_mask) noexcept :
        m_color_formats(color_formats),
        m_depth_format(depth_format),
        m_stencil_format(stencil_format),
        m_view_mask(view_mask) {}
    DynamicRenderScopeInfo(const DynamicRenderScopeInfo &) = default;
    DynamicRenderScopeInfo(DynamicRenderScopeInfo &&) noexcept = default;
    DynamicRenderScopeInfo & operator=(const DynamicRenderScopeInfo &) = default;
    DynamicRenderScopeInfo & operator=(DynamicRenderScopeInfo &&) noexcept = default;
public:
    const FormatListView & getColorFormats() const noexcept { return m_color_formats; }
    const vk::Format & getDepthFormat() const noexcept { return m_depth_format; }
    const vk::Format & getStencilFormat() const noexcept { return m_stencil_format; }
    const uint32_t & getViewMask() const noexcept { return m_view_mask; }
private:
    FormatListView m_color_formats;
    vk::Format m_depth_format;
    vk::Format m_stencil_format;
    uint32_t m_view_mask;
};

class DynamicRender
{
    using Self = DynamicRender;
    using Root = vk::RenderingInfo;
    using AttachmentInfoList = std::vector<vk::RenderingAttachmentInfo>;
    using ResolveIndices = std::vector<uint32_t>;
    using FormatList = std::vector<vk::Format>;
    struct BarrierSlot
    {
        uint32_t slot_index = vk::AttachmentUnused;
        uint32_t entry_index = vk::AttachmentUnused;
        uint32_t exit_index = vk::AttachmentUnused;
    };
    using BarrierSlotList = std::vector<BarrierSlot>;
    using BarrierList = std::vector<vk::ImageMemoryBarrier2>;
    using RequiredImageUsageList = std::vector<vk::ImageUsageFlags>;
public:
    ~DynamicRender() noexcept = default;
    DynamicRender() noexcept = default;
    DynamicRender(const Self &) = delete;
    DynamicRender(Self &&) noexcept = default;
    Self & operator=(const Self &) = delete;
    Self & operator=(Self &&) noexcept = default;
public:
    std::error_code create(const DynamicRenderInfo & render_info) noexcept;
    void begin(CommandBufferProxy & cmd, const RenderTarget & render_target) noexcept;
    void end(CommandBufferProxy & cmd) noexcept;
    DynamicRenderScopeInfo makeScopeInfo() const noexcept
    {
        return {m_color_formats, m_depth_format, m_stencil_format, m_rendering.root().viewMask};
    }
private:
    utils::DynamicStructureChain<Root> m_rendering;
    ResolveIndices m_resolve_indices;
    AttachmentInfoList m_color_attachments;
    FormatList m_color_formats;
    vk::RenderingAttachmentInfo m_depth_attachment;
    vk::RenderingAttachmentInfo m_stencil_attachment;
    vk::Format m_depth_format = vk::Format::eUndefined;
    vk::Format m_stencil_format = vk::Format::eUndefined;
    BarrierSlotList m_barrier_slots;
    BarrierList m_entry_barriers;
    BarrierList m_exit_barriers;
};

} // namespace lcf::vkc