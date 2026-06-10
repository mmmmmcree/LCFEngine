#pragma once

#include "vk_core/config/FeatureDeclare.h"
#include <array>

namespace lcf::vkc::surf {

inline constexpr std::array k_swapchain_maintenance_device_extensions {
    vk::EXTSwapchainMaintenance1ExtensionName,
};

inline constexpr std::array k_swapchain_maintenance_features {
    conf::k_feature<&vk::PhysicalDeviceSwapchainMaintenance1FeaturesEXT::swapchainMaintenance1>,
};

inline constexpr conf::FeatureDependency k_swapchain_maintenance_dependency {
    .device_extensions = k_swapchain_maintenance_device_extensions,
    .features = k_swapchain_maintenance_features,
};

inline constexpr std::array k_module_instance_extensions {
    vk::KHRSurfaceExtensionName,
};

inline constexpr std::array k_module_device_extensions {
    vk::KHRSwapchainExtensionName,
};

inline constexpr std::array k_module_optional {
    &k_swapchain_maintenance_dependency,
};

// platform window-system instance extensions (VK_KHR_win32_surface, ...) are
// provided by the GUI side together with SurfaceProvider, not by this manifest
inline constexpr conf::FeatureDependency k_module_dependency {
    .instance_extensions = k_module_instance_extensions,
    .device_extensions = k_module_device_extensions,
    .optional = k_module_optional,
};

} // namespace lcf::vkc::surf
