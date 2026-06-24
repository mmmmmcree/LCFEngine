#pragma once

namespace lcf::vkc {

class InstanceExtensionManifest;

class DeviceExtensionManifest;

} // namespace lcf::vkc

namespace lcf::vkc {

void register_memory_module(InstanceExtensionManifest & manifest) noexcept;

void register_buffer_device_address(DeviceExtensionManifest & manifest) noexcept;

} // namespace lcf::vkc
