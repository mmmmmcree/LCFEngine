#pragma once

#include <vulkan/vulkan.hpp>
#include <expected>

namespace lcf::vkc::bs {

class PhysicalDeviceSelectInfo;

std::expected<vk::PhysicalDevice, std::error_code> select_physical_device(vk::Instance instance, const PhysicalDeviceSelectInfo & info) noexcept;

} // namespace lcf::vkc::bs