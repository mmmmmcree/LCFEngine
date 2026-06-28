#pragma once

#include <cstdint>
#include <vulkan/vulkan_enums.hpp>

namespace lcf::vkc {

using CommandBufferUsageFlagBits = vk::CommandPoolCreateFlagBits;

using CommandBufferUsageFlags = vk::CommandPoolCreateFlags;

} // namespace lcf::vkc
