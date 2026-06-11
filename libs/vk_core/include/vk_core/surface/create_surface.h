#pragma once

#include "WindowHandle.h"
#include <vulkan/vulkan.hpp>

namespace lcf::vkc::surf {

vk::UniqueSurfaceKHR create_surface(
    vk::Instance instance,
    const WindowHandle & window_handle,
    const vk::AllocationCallbacks * allocator = nullptr);

} // namespace lcf::vkc::surf
