#pragma once

#include <vulkan/vulkan_enums.hpp>

namespace lcf::vkc::utils {

inline bool is_depth_format(vk::Format format) noexcept
{
    return format >= vk::Format::eD16Unorm and format <= vk::Format::eD32SfloatS8Uint and format != vk::Format::eS8Uint;
}

inline bool is_stencil_format(vk::Format format) noexcept
{
    return format >= vk::Format::eS8Uint and format <= vk::Format::eD32SfloatS8Uint;
}

inline bool is_depth_stencil_format(vk::Format format) noexcept
{
    return format >= vk::Format::eD16UnormS8Uint and format <= vk::Format::eD32SfloatS8Uint;
}

} // namespace lcf::vkc::utils