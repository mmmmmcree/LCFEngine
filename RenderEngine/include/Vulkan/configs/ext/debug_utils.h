#pragma once

#include <vulkan/vulkan.hpp>

namespace lcf::render::vkext {

    void enable_debug_utils(vk::Instance instance);

    void release_debug_utils_resources() noexcept;

} // namespace lcf::render::vkext
