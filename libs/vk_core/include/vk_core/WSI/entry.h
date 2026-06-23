#pragma once

namespace lcf::vkc {

class InstanceExtensionManifest;

class DeviceExtensionManifest;

} // namespace lcf::vkc::mnf

namespace lcf::vkc::wsi {

void register_surface(InstanceExtensionManifest & manifest) noexcept;

void register_swapchain(DeviceExtensionManifest & manifest) noexcept;

namespace compat {

void register_swapchain(DeviceExtensionManifest & manifest) noexcept;

} // namespace lcf::vkc::wsi::compat

} // namespace lcf::vkc::surf