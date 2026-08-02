#pragma once

#include "enums/enum_attributes_traits.h"
#include <vulkan/vulkan.hpp>

namespace lcf::vkc {

enum class AttachmentUsage : uint8_t
{
    eNone,
    eDiscard,
    eColorAttachment,
    eDepthStencilAttachment,
    eDepthStencilReadOnly,
    eTransferSource,
    eTransferDestination,
    eShaderRead,
    ePresent,
};

} // namespace lcf::vkc

template <>
struct lcf::enum_specialized_attributes_traits<lcf::vkc::AttachmentUsage>
{
private:
    using AttachmentUsage = lcf::vkc::AttachmentUsage;
    struct Attributes
    {
        vk::ImageLayout specific_layout;
        vk::ImageLayout unified_layout;
        vk::ImageUsageFlags required_image_usage;
        vk::PipelineStageFlags2 stage_mask;
        vk::AccessFlags2 access_mask;
    };
    static constexpr Attributes attributes_list[] = {
        { //- eNone
            vk::ImageLayout::eUndefined, vk::ImageLayout::eUndefined, {}, {},
        },
        { //- eDiscard
            vk::ImageLayout::eUndefined, vk::ImageLayout::eUndefined, {}, vk::PipelineStageFlagBits2::eAllCommands, vk::AccessFlagBits2::eNone 
        },
        { //- eColorAttachment
            vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eGeneral,
            vk::ImageUsageFlagBits::eColorAttachment,
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentRead,
        },
        { //- eDepthStencilAttachment
            vk::ImageLayout::eDepthStencilAttachmentOptimal, vk::ImageLayout::eGeneral,
            vk::ImageUsageFlagBits::eDepthStencilAttachment,
            vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
            vk::AccessFlagBits2::eDepthStencilAttachmentWrite | vk::AccessFlagBits2::eDepthStencilAttachmentRead,
        },
        { //- eDepthStencilReadOnly
            vk::ImageLayout::eDepthStencilReadOnlyOptimal, vk::ImageLayout::eGeneral,
            vk::ImageUsageFlagBits::eDepthStencilAttachment,
            vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests |
            vk::PipelineStageFlagBits2::eFragmentShader,
            vk::AccessFlagBits2::eDepthStencilAttachmentRead,
        },
        { //- eTransferSource
            vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eGeneral,
            vk::ImageUsageFlagBits::eTransferSrc,
            vk::PipelineStageFlagBits2::eAllTransfer,
            vk::AccessFlagBits2::eTransferRead,
        },
        { //- eTransferDestination
            vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eGeneral,
            vk::ImageUsageFlagBits::eTransferDst,
            vk::PipelineStageFlagBits2::eAllTransfer,
            vk::AccessFlagBits2::eTransferWrite,
        },
        { //- eShaderRead
            vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eGeneral,
            vk::ImageUsageFlagBits::eSampled,
            vk::PipelineStageFlagBits2::eAllGraphics | vk::PipelineStageFlagBits2::eComputeShader,
            vk::AccessFlagBits2::eShaderSampledRead,
        },
        { //- ePresent: synchronized by semaphores; the barrier only carries the layout
            vk::ImageLayout::ePresentSrcKHR, vk::ImageLayout::ePresentSrcKHR, {}, {},
        },
    };
    static constexpr const Attributes & get_attributes(lcf::vkc::AttachmentUsage usage) noexcept
    {
        return attributes_list[std::to_underlying(usage)];
    }
public:
    static constexpr vk::ImageLayout layout_of(AttachmentUsage usage, bool unified_enabled = false) noexcept
    {
        const auto & attributes = get_attributes(usage);
        return unified_enabled ? attributes.unified_layout : attributes.specific_layout;
    }
    template <lcf::vkc::AttachmentUsage usage>
    static constexpr vk::ImageLayout layout_of(bool unified_enabled = false) noexcept
    {
        return layout_of(usage, unified_enabled); 
    }
    static constexpr vk::ImageUsageFlags required_image_usage_of(AttachmentUsage usage) noexcept { return get_attributes(usage).required_image_usage; }
    template <AttachmentUsage usage>
    static constexpr vk::ImageUsageFlags required_image_usage_of() noexcept { return required_image_usage_of(usage); }
    static constexpr vk::PipelineStageFlags2 stage_mask_of(AttachmentUsage usage) noexcept { return get_attributes(usage).stage_mask; }
    template <AttachmentUsage usage>
    static constexpr vk::PipelineStageFlags2 stage_mask_of() noexcept { return stage_mask_of(usage); }
    static constexpr vk::AccessFlags2 access_mask_of(AttachmentUsage usage) noexcept { return get_attributes(usage).access_mask; }
    template <AttachmentUsage usage>
    static constexpr vk::AccessFlags2 access_mask_of() noexcept { return access_mask_of(usage); }
};