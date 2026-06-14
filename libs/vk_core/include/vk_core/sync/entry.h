#pragma once

namespace lcf::vkc {

class InstanceExtensionManifest;

class DeviceExtensionManifest;

} // namespace lcf::vkc

namespace lcf::vkc::sync {

void register_sync_module(InstanceExtensionManifest & manifest) noexcept;

void register_timeline_semaphore(DeviceExtensionManifest & manifest) noexcept;

} // namespace lcf::vkc::sync