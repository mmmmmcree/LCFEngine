#pragma once

#include <vulkan/vulkan.hpp>
#include <expected>
#include "vk_core/error.h"

namespace lcf::vkc::bs {

class InstanceCreateInfo;

//! @param warning_out optional; receives a non-fatal diagnostic (missing instance layers) when one occurs.
std::expected<vk::UniqueInstance, Error> create_instance(const InstanceCreateInfo & create_info, Error * warning_out = nullptr) noexcept;

} // namespace lcf::vkc::bs
