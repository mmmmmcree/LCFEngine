#pragma once

#include "vk_core/probe/probe.h"
#include "vk_core/WSI/entry.h"

namespace lcf::vkc::wsi {

struct SwapchainCapability {};

const probe::CapabilityDescriptor & capability_descriptor(SwapchainCapability) noexcept;

template<typename = void>
inline void register_probed(
    SwapchainCapability,
    InstanceExtensionManifest & instance_manifest,
    DeviceExtensionManifest & device_manifest) noexcept
{
    (void)instance_manifest;
    (void)device_manifest;
}

} // namespace lcf::vkc::wsi
