#pragma once

#include "vk_core/config/FeatureDeclare.h"
#include <array>

namespace lcf::vkc::sync {

inline constexpr std::array k_module_features {
    conf::k_feature<&vk::PhysicalDeviceVulkan12Features::timelineSemaphore>,
};

// using the sync module means using TimelineSemaphore, so its feature is a
// required dependency of the module itself
inline constexpr conf::FeatureDependency k_module_dependency {
    .core_since = vk::ApiVersion12,
    .features = k_module_features,
};

} // namespace lcf::vkc::sync
