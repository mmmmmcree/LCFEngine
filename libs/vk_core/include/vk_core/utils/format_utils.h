#pragma once

#include <vulkan/vulkan_enums.hpp>

namespace lcf::vkc::utils {

inline constexpr bool is_depth_format(vk::Format format) noexcept
{
    return format >= vk::Format::eD16Unorm and format <= vk::Format::eD32SfloatS8Uint and format != vk::Format::eS8Uint;
}

inline constexpr bool is_stencil_format(vk::Format format) noexcept
{
    return format >= vk::Format::eS8Uint and format <= vk::Format::eD32SfloatS8Uint;
}

inline bool is_depth_stencil_format(vk::Format format) noexcept
{
    return format >= vk::Format::eD16UnormS8Uint and format <= vk::Format::eD32SfloatS8Uint;
}

inline constexpr bool is_discarding_load_op(vk::AttachmentLoadOp op) noexcept
{
    return op == vk::AttachmentLoadOp::eClear or op == vk::AttachmentLoadOp::eDontCare;
}

inline constexpr bool is_discarding_store_op(vk::AttachmentStoreOp op) noexcept
{
    //- eNone is not a discard: it leaves contents either preserved or undefined, and treating
    //- "maybe preserved" as discardable would throw away data the caller may still want
    return op == vk::AttachmentStoreOp::eDontCare;
}

inline constexpr bool discards_on_load(vk::Format format, vk::AttachmentLoadOp load_op, vk::AttachmentLoadOp stencil_load_op) noexcept
{
    bool load_discards = is_discarding_load_op(load_op);
    if (not is_stencil_format(format)) { return load_discards; }  //- color or depth-only
    bool stencil_load_discards = is_discarding_load_op(stencil_load_op);
    if (not is_depth_format(format)) { return stencil_load_discards; }   //- stencil-only
    return load_discards and stencil_load_discards;
}

inline constexpr bool discards_on_store(vk::Format format, vk::AttachmentStoreOp store_op, vk::AttachmentStoreOp stencil_store_op) noexcept
{
    bool store_discards = is_discarding_store_op(store_op);
    if (not is_stencil_format(format)) { return store_discards; }
    bool stencil_store_discards = is_discarding_store_op(stencil_store_op);
    if (not is_depth_format(format)) { return stencil_store_discards; }
    return store_discards and stencil_store_discards;
}

inline constexpr vk::ImageLayout to_unified_image_layout(vk::ImageLayout specific_layout) noexcept
{
    vk::ImageLayout unified_layout = vk::ImageLayout::eGeneral;
    switch (specific_layout) {
        case vk::ImageLayout::eUndefined : 
        case vk::ImageLayout::ePresentSrcKHR : { unified_layout = specific_layout; } break;
        default : break;
    }
    return unified_layout;
}

} // namespace lcf::vkc::utils