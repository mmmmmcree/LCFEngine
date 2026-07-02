#pragma once

#include <vulkan/vulkan.hpp>

namespace lcf::vkc {

class InstanceExtensionManifest;

namespace dbg {

class DebugLogCallbacks;

} // namespace lcf::vkc::dbg

} // namespace lcf::vkc

namespace lcf::vkc::entry {

void register_debug_utils(InstanceExtensionManifest & manifest) noexcept;

void register_debug_utils(
    InstanceExtensionManifest & manifest,
    vk::DebugUtilsMessageSeverityFlagsEXT severity,
    const dbg::DebugLogCallbacks & callbacks) noexcept;

} // namespace lcf::vkc::entry
