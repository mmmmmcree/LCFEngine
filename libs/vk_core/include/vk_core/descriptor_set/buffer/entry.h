#pragma once

namespace lcf::vkc {

class DeviceExtensionManifest;

} // namespace lcf::vkc

namespace lcf::vkc::entry {

void register_descriptor_buffer(DeviceExtensionManifest & manifest) noexcept;

} // namespace lcf::vkc::entry
