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
    //todo optional
    static constexpr std::array k_surface_maintenance1_extensions
    {
        vk::KHRGetSurfaceCapabilities2ExtensionName,
        vk::KHRSurfaceMaintenance1ExtensionName,
    };
    // if constexpr xxx
    manifest.addRequiredExtensions(k_surface_maintenance1_extensions);
}

void register_swapchain(DeviceExtensionManifest & manifest) noexcept
{
    static constexpr std::array k_extensions
    {
        vk::KHRSwapchainExtensionName,
    };
    manifest.addRequiredExtensions(k_extensions);
    
    // todo optional
    static constexpr std::array k_swapchain_maintenance1_extensions
    {
        vk::KHRSwapchainMaintenance1ExtensionName
    };
    static constexpr std::array k_swapchain_maintenance1_features
    {
        utils::t_feature_bit<&vk::PhysicalDeviceSwapchainMaintenance1FeaturesEXT::swapchainMaintenance1>,
    };
    // if constexpr xxx
    manifest.addRequiredExtensions(k_swapchain_maintenance1_extensions)
        .addRequiredFeatures(k_swapchain_maintenance1_features);
}

} // namespace lcf::vkc::sync