#include "vk_core/surface/entry.h"
#include "vk_core/manifest/InstanceExtensionManifest.h"
#include "vk_core/manifest/DeviceExtensionManifest.h"
#include <array>

namespace lcf::vkc::surf {

void register_surface(InstanceExtensionManifest & manifest) noexcept
{
    static constexpr std::array k_extensions
    {
        vk::KHRSurfaceExtensionName,
    #if defined(VK_USE_PLATFORM_WIN32_KHR)
        vk::KHRWin32SurfaceExtensionName,
    #endif
    #if defined(VK_USE_PLATFORM_XCB_KHR)
        vk::KHRXcbSurfaceExtensionName,
    #endif
    #if defined(VK_USE_PLATFORM_WAYLAND_KHR)
        vk::KHRWaylandSurfaceExtensionName,
    #endif
    #if defined(VK_USE_PLATFORM_METAL_EXT)
        vk::EXTMetalSurfaceExtensionName,
    #endif
    };
    manifest.addRequiredExtensions(k_extensions);
}

void register_swapchain(DeviceExtensionManifest & manifest) noexcept
{
    static constexpr std::array k_extensions
    {
        vk::KHRSwapchainExtensionName,
        vk::EXTSwapchainMaintenance1ExtensionName,
    };
    static constexpr std::array k_features
    {
        utils::t_feature_bit<&vk::PhysicalDeviceSwapchainMaintenance1FeaturesEXT::swapchainMaintenance1>,
    };
    manifest.addRequiredExtensions(k_extensions)
        .addRequiredFeatures(k_features);
}

} // namespace lcf::vkc::sync