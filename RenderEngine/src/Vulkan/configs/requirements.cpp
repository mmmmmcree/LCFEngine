#include "Vulkan/configs/requirements.h"
#include <ranges>

namespace stdr = std::ranges;

// ============================================================================
//  Instance Extensions
// ============================================================================

static constexpr const char * k_instance_extensions[] = {
#ifndef NDEBUG
    VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
    VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
    VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME,
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
    VK_EXT_DEVICE_GENERATED_COMMANDS_EXTENSION_NAME
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

    const std::set<std::string_view> & get_instance_extensions() noexcept
    {
        static const auto s_instance_extensions { k_instance_extensions | stdr::to<std::set<std::string_view>>() };
        return s_instance_extensions;
    }

    const std::set<std::string_view> & get_instance_layers() noexcept
    {
        static const auto s_instance_layers { k_instance_layers | stdr::to<std::set<std::string_view>>() };
        return s_instance_layers;
    }

    const std::set<std::string_view> & get_device_extensions() noexcept
    {
        static const auto s_device_extensions { k_device_extensions | stdr::to<std::set<std::string_view>>() };
        return s_device_extensions;
    }

    const std::set<std::string_view> & get_presentation_device_extensions() noexcept
    {
        static const auto s_presentation_device_extensions { k_presentation_device_extensions | stdr::to<std::set<std::string_view>>() };
        return s_presentation_device_extensions;
    }

} // namespace lcf::render::vkreq
