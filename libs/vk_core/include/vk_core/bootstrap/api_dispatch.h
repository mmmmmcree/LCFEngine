#pragma once

#include <vulkan/vulkan.hpp>

namespace lcf::vkc::bs {

    //! Call once at program startup, before any Vulkan API call.
    void initialize_loader() noexcept;

    //! Call after vk::createInstance. Loads instance-level function pointers.
    void initialize_instance(vk::Instance instance) noexcept;

    //! Call after vk::createDevice. Loads device-level function pointers.
    void initialize_device(vk::Device device) noexcept;

} // namespace lcf::vkc::bs