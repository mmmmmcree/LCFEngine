#pragma once

namespace lcf::vkc {

class InstanceExtensionManifest;

class DeviceExtensionManifest;

} // namespace lcf::vkc

namespace lcf::vkc::entry {

void register_queue(InstanceExtensionManifest & inst_manifest, DeviceExtensionManifest & device_manifest) noexcept;

} // namespace lcf::vkc::entry