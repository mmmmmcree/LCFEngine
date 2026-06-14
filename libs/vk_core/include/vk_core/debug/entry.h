#pragma once

namespace lcf::vkc {

class InstanceExtensionManifest;

} // namespace lcf::vkc::mnf

namespace lcf::vkc::dbg {

void register_debug_utils(InstanceExtensionManifest & manifest) noexcept;

} // namespace lcf::vkc::dbg