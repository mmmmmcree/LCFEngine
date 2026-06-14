#pragma once

#include <vulkan/vulkan.hpp>
#include <expected>

namespace lcf::vkc::bs {

class DeviceCreateInfo;

std::expected<vk::UniqueDevice, std::error_code> create_device(vk::PhysicalDevice physical_device, const DeviceCreateInfo &create_info) noexcept;



} // namespace lcf::vkc
