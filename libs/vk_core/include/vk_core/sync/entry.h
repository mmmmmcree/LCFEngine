#pragma once

namespace lcf::vkc {

class InstanceExtensionManifest;

class DeviceExtensionManifest;

void register_timeline_semaphore(DeviceExtensionManifest & manifest) noexcept;

} // namespace lcf::vkc