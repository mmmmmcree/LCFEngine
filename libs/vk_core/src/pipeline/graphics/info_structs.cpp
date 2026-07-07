#include "vk_core/pipeline/graphics/info_structs.h"
#include <ranges>
#include <utility>

namespace stdr = std::ranges;
namespace stdv = std::views;

namespace lcf::vkc {

bool RenderTargetInfo::hasAnyResolve() const noexcept
{
    return std::to_underlying(m_sample_count) > 1
        and stdr::any_of(m_color_resolve_modes, [](auto m) { return m != vk::ResolveModeFlagBits::eNone; });
}

auto VertexInputInfo::genBindingDescriptions2EXT() const -> BindingList2EXT
{
    auto to_2ext = [this](const auto & indexed) -> vk::VertexInputBindingDescription2EXT {
        const auto & [i, binding] = indexed;
        return { binding.binding, binding.stride, binding.inputRate, std::max(1u, m_binding_divisors[i]) };
    };
    return m_bindings | stdv::enumerate | stdv::transform(to_2ext) | stdr::to<BindingList2EXT>();
}

auto VertexInputInfo::genAttributeDescriptions2EXT() const -> AttributeList2EXT
{
    auto to_2ext = [](const vk::VertexInputAttributeDescription & attribute) {
        return vk::VertexInputAttributeDescription2EXT { attribute.location, attribute.binding, attribute.format, attribute.offset };
    };
    return m_attributes | stdv::transform(to_2ext) | stdr::to<AttributeList2EXT>();
}

SubpassDescriptionInfo::operator vk::SubpassDescription2() const noexcept
{
    auto to_ref2 = [](const AttachmentReferenceInfo & reference) { return static_cast<vk::AttachmentReference2>(reference); };
    m_input_scratch = m_input_references | stdv::transform(to_ref2) | stdr::to<ReferenceList2>();
    m_color_scratch = m_color_references | stdv::transform(to_ref2) | stdr::to<ReferenceList2>();
    m_resolve_scratch = m_resolve_references | stdv::transform(to_ref2) | stdr::to<ReferenceList2>();

    vk::SubpassDescription2 subpass { m_flags, m_bind_point, m_view_mask };
    subpass.setInputAttachments(m_input_scratch).setColorAttachments(m_color_scratch);
    if (not m_resolve_scratch.empty()) { subpass.setPResolveAttachments(m_resolve_scratch.data()); }
    if (m_depth_stencil_reference.has_value()) {
        m_depth_stencil_scratch = *m_depth_stencil_reference;
        subpass.setPDepthStencilAttachment(&m_depth_stencil_scratch);
    }
    subpass.setPreserveAttachments(m_preserve_attachments);
    return subpass;
}

} // namespace lcf::vkc
