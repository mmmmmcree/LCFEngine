#pragma once

namespace lcf::vkc {

class InstanceExtensionManifest;

class DeviceExtensionManifest;

} // namespace lcf::vkc

namespace lcf::vkc::entry {

void register_memory(InstanceExtensionManifest & manifest) noexcept;

void register_buffer_device_address(DeviceExtensionManifest & manifest) noexcept;

} // namespace lcf::vkc::entry
