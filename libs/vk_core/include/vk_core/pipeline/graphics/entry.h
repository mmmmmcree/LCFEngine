#pragma once

namespace lcf::vkc {

class InstanceExtensionManifest;

class DeviceExtensionManifest;

} // namespace lcf::vkc

namespace lcf::vkc::entry {

void register_dynamic_render(DeviceExtensionManifest & manifest) noexcept;

} // namespace lcf::vkc::entry