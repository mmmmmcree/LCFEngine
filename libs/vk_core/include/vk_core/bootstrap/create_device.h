#pragma once

#include <vulkan/vulkan.hpp>
#include <expected>
#include "vk_core/error.h"

namespace lcf::vkc::bs {

class DeviceCreateInfo;

std::expected<vk::UniqueDevice, Error> create_device(vk::PhysicalDevice physical_device, const DeviceCreateInfo &create_info) noexcept;



} // namespace lcf::vkc
