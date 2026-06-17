#pragma once

namespace lcf::vkc {

class InstanceExtensionManifest;

class DeviceExtensionManifest;

} // namespace lcf::vkc

namespace lcf::vkc::sync {

void register_context_module(InstanceExtensionManifest & inst_manifest, DeviceExtensionManifest & device_manifest) noexcept;

} // namespace lcf::vkc::sync