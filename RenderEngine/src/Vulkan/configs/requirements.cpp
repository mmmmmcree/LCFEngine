#include "Vulkan/configs/requirements.h"

// ============================================================================
//  Instance Extensions
// ============================================================================

static constexpr const char * k_instance_extensions[] = {
#ifndef NDEBUG
    VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
};

// ============================================================================
//  Instance Layers
// ============================================================================

static constexpr const char * k_instance_layers[] = {
#ifndef NDEBUG
    "VK_LAYER_KHRONOS_validation",
#endif
};

// ============================================================================
//  Device Extensions (core, always required)
// ============================================================================

static constexpr const char * k_device_extensions[] = {
    VK_KHR_MAINTENANCE1_EXTENSION_NAME,
    VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
    VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME,
    VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME,
};

// ============================================================================
//  Device Extensions (presentation, only when surfaces are present)
// ============================================================================

static constexpr const char * k_presentation_device_extensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME,
};

// ============================================================================

namespace lcf::render::vkreq {

std::span<const char * const> get_instance_extensions() noexcept { return k_instance_extensions; }
std::span<const char * const> get_instance_layers() noexcept { return k_instance_layers; }
std::span<const char * const> get_device_extensions() noexcept { return k_device_extensions; }
std::span<const char * const> get_presentation_device_extensions() noexcept { return k_presentation_device_extensions; }

} // namespace lcf::render::vkreq
