#pragma once

#include "resource_utils.h"
#include <vulkan/vulkan.hpp> 
#include <vector>

namespace lcf::vkc::details {

    std::vector<ResourceLease> enable_instance_extensions(vk::Instance instance) noexcept;

} // namespace lcf::vkc::details