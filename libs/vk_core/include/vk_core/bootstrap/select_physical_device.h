#pragma once

#include <vulkan/vulkan.hpp>
#include <expected>
#include "vk_core/error.h"

namespace lcf::vkc::bs {

class PhysicalDeviceSelectInfo;

std::expected<vk::PhysicalDevice, Error> select_physical_device(vk::Instance instance, const PhysicalDeviceSelectInfo & info) noexcept;

} // namespace lcf::vkc::bs