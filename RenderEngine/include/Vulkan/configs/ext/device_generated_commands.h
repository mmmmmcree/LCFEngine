#pragma once

#include <vulkan/vulkan.hpp>

namespace lcf::render::vkext {

    void enable_device_generated_commands(vk::Device device);

    void release_device_generated_commands_resources() noexcept;

} // namespace lcf::render::vkext
