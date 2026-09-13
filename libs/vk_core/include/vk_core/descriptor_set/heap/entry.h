#pragma once

namespace lcf::vkc {

class DeviceExtensionManifest;

} // namespace lcf::vkc

namespace lcf::vkc::entry {

void register_descriptor_heap(DeviceExtensionManifest & manifest) noexcept;
void register_descriptor_heap_untyped(DeviceExtensionManifest & manifest) noexcept;

} // namespace lcf::vkc::entry
