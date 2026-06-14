#pragma once

#include <vulkan/vulkan.hpp>
#include "resource_utils.h"

namespace lcf::vkc::dbg {

ResourceLease enable_debug_utils(vk::Instance instance) noexcept;

} // namespace lcf::vkc::details