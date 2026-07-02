#pragma once

namespace lcf::vkc {

class InstanceExtensionManifest;

class DeviceExtensionManifest;

} // namespace lcf::vkc

namespace lcf::vkc::entry {

void register_timeline_semaphore(DeviceExtensionManifest & manifest) noexcept;

} // namespace lcf::vkc::entry