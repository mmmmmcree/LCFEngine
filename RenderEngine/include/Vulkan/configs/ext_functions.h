#pragma once

#include <vulkan/vulkan.hpp>

namespace lcf::render::vkext {

    void load_instance_extensions(vk::Instance instance);

    void release_instance_extensions_resources() noexcept;

    void load_device_extensions(vk::Device device);

    void release_device_extensions_resources() noexcept;

} // namespace lcf::render::vkext
