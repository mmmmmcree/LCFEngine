#pragma once

namespace lcf::vkc {

class InstanceExtensionManifest;

class DeviceExtensionManifest;

} // namespace lcf::vkc

namespace lcf::vkc::entry {

void register_surface(InstanceExtensionManifest & manifest) noexcept;

void register_swapchain(DeviceExtensionManifest & manifest) noexcept;

void register_compat_swapchain(DeviceExtensionManifest & manifest) noexcept;

} // namespace lcf::vkc::entry