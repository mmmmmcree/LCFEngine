#pragma once

#include "vk_core/WSI/Swapchain.h"
#include "vk_core/WSI/probed/entry.h"

namespace lcf::vkc::wsi {

inline void register_probed(
    SwapchainCapability,
    InstanceExtensionManifest & instance_manifest,
    DeviceExtensionManifest & device_manifest) noexcept
{
    entry::register_surface(instance_manifest);
    entry::register_swapchain(instance_manifest, device_manifest);
}

namespace probed {

using Swapchain = lcf::vkc::wsi::Swapchain;

} // namespace probed

} // namespace lcf::vkc::wsi
