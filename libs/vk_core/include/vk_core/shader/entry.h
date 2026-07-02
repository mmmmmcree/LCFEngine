#pragma once

namespace lcf::vkc {

class InstanceExtensionManifest;

class DeviceExtensionManifest;

} // namespace lcf::vkc

namespace lcf::vkc::entry {

void register_shader_object(DeviceExtensionManifest & manifest) noexcept;

} // namespace lcf::vkc::entry