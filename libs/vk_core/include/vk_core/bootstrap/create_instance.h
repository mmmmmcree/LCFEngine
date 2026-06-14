#pragma once

#include <vulkan/vulkan.hpp>
#include <expected>

namespace lcf::vkc::bs {

class InstanceCreateInfo;

std::expected<vk::UniqueInstance, std::error_code> create_instance(const InstanceCreateInfo & create_info) noexcept;

} // namespace lcf::vkc::bs
