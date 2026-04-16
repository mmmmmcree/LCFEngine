#pragma once

#include <vulkan/vulkan.hpp>

namespace lcf::render::vkdispatch {

    //! Call once at program startup, before any Vulkan API call.
    void initialize_loader();

    //! Call after vk::createInstance. Loads instance-level function pointers.
    void initialize_instance(vk::Instance instance);

    //! Call after vk::createDevice. Loads device-level function pointers.
    void initialize_device(vk::Device device);

} // namespace lcf::render::vkdispatch