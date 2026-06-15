#pragma once

#include <vulkan/vulkan.hpp>

namespace lcf::vkc {

class InstanceExtensionManifest;

} // namespace lcf::vkc

namespace lcf::vkc::dbg {

class DebugLogCallbacks;


void register_debug_utils(InstanceExtensionManifest & manifest) noexcept;

void register_debug_utils(
    InstanceExtensionManifest & manifest,
    vk::DebugUtilsMessageSeverityFlagsEXT severity,
    const DebugLogCallbacks & callbacks) noexcept;

} // namespace lcf::vkc::dbg
