#pragma once

#include "WindowHandle.h"
#include <vulkan/vulkan.hpp>
#include <expected>

namespace lcf::vkc::wsi {

std::expected<vk::UniqueSurfaceKHR, std::error_code> create_surface(
    vk::Instance instance,
    const WindowHandle & window_handle,
    const vk::AllocationCallbacks * allocator = nullptr) noexcept;

} // namespace lcf::vkc::surf
