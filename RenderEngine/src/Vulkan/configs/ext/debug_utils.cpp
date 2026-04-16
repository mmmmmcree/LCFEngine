#include "Vulkan/configs/ext/debug_utils.h"
#include "log.h"
#include <format>

// ============================================================================
//  Static dispatch: extension functions must be loaded manually via getProcAddr
// ============================================================================

#if !VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
static PFN_vkCreateDebugUtilsMessengerEXT  s_pfnCreateDebugUtilsMessengerEXT  = nullptr;
static PFN_vkDestroyDebugUtilsMessengerEXT s_pfnDestroyDebugUtilsMessengerEXT = nullptr;

VKAPI_ATTR VkResult VKAPI_CALL vkCreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT * pCreateInfo,
    const VkAllocationCallbacks * pAllocator,
    VkDebugUtilsMessengerEXT * pMessenger)
{
    return s_pfnCreateDebugUtilsMessengerEXT(instance, pCreateInfo, pAllocator, pMessenger);
}

VKAPI_ATTR void VKAPI_CALL vkDestroyDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerEXT messenger,
    const VkAllocationCallbacks * pAllocator)
{
    return s_pfnDestroyDebugUtilsMessengerEXT(instance, messenger, pAllocator);
}
#endif // !VULKAN_HPP_DISPATCH_LOADER_DYNAMIC

// ============================================================================
//  Debug callback
// ============================================================================

static VKAPI_ATTR vk::Bool32 VKAPI_CALL debug_callback(
    vk::DebugUtilsMessageSeverityFlagBitsEXT severity_flags,
    vk::DebugUtilsMessageTypeFlagsEXT type_flags,
    const vk::DebugUtilsMessengerCallbackDataEXT * callback_data,
    void * user_data)
{
    (void)user_data;
    if (not callback_data->pMessage) { return false; }
    std::string log_output = std::format(
        "{:=^80}\n"
        "[Type] {}\n"
        "[Message ID] (0x{:x}) {}\n"
        "[Message] {}\n",
        "Vulkan Validation Layer",
        vk::to_string(type_flags),
        static_cast<uint32_t>(callback_data->messageIdNumber),
        callback_data->pMessageIdName,
        callback_data->pMessage);
    if (callback_data->queueLabelCount > 0) {
        log_output += std::format("<Queue Labels>\n");
        for (uint32_t i = 0; i < callback_data->queueLabelCount; ++i) {
            const auto & label = callback_data->pQueueLabels[i];
            log_output += std::format(
                "  [{}] Name: {}, Color: [{:.3f}, {:.3f}, {:.3f}, {:.3f}]\n",
                i,
                label.pLabelName ? label.pLabelName : "Unnamed",
                label.color[0], label.color[1], label.color[2], label.color[3]);
        }
    }
    if (callback_data->cmdBufLabelCount > 0) {
        log_output += std::format("<Command Buffer Labels>\n");
        for (uint32_t i = 0; i < callback_data->cmdBufLabelCount; ++i) {
            const auto & label = callback_data->pCmdBufLabels[i];
            log_output += std::format(
                "  [{}] Name: {}, Color: [{:.3f}, {:.3f}, {:.3f}, {:.3f}]\n",
                i,
                label.pLabelName ? label.pLabelName : "Unnamed",
                label.color[0], label.color[1], label.color[2], label.color[3]);
        }
    }
    if (callback_data->objectCount > 0) {
        log_output += std::format("<Related Objects>\n");
        for (uint32_t i = 0; i < callback_data->objectCount; ++i) {
            const auto & obj = callback_data->pObjects[i];
            log_output += std::format(
                "  [{}] Type: {}, Handle: 0x{:x}, Name: {}\n",
                i,
                vk::to_string(obj.objectType),
                obj.objectHandle,
                obj.pObjectName ? obj.pObjectName : "Unnamed");
        }
    }
    if (severity_flags & vk::DebugUtilsMessageSeverityFlagBitsEXT::eError) { lcf_log_error(log_output); }
    else if (severity_flags & vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning) { lcf_log_warn(log_output); }
    // else if (severity_flags & vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo) { lcf_log_info(log_output); }
    // else if (severity_flags & vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose) { lcf_log_trace(log_output); }
    return false;
}

// ============================================================================
//  RenderDoc environment detection
// ============================================================================

static bool is_renderdoc_environment()
{
#ifdef _MSC_VER
    char * val = nullptr;
    size_t len = 0;
    _dupenv_s(&val, &len, "ENABLE_VULKAN_RENDERDOC_CAPTURE");
    bool result = val != nullptr;
    free(val);
    return result;
#else
    return std::getenv("ENABLE_VULKAN_RENDERDOC_CAPTURE") != nullptr;
#endif
}

// ============================================================================
//  Debug messenger state
// ============================================================================

static vk::UniqueDebugUtilsMessengerEXT s_debug_messenger;

void lcf::render::vkext::enable_debug_utils(vk::Instance instance)
{
#if !VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
    s_pfnCreateDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        instance.getProcAddr("vkCreateDebugUtilsMessengerEXT"));
    s_pfnDestroyDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        instance.getProcAddr("vkDestroyDebugUtilsMessengerEXT"));
#endif

    if (is_renderdoc_environment()) {
        lcf_log_warn("RenderDoc environment detected — skipping debug messenger creation");
        return; //! RenderDoc is contradictory to custom debug callback
    }

    vk::DebugUtilsMessengerCreateInfoEXT debug_messenger_info;
    debug_messenger_info.setMessageSeverity(vk::FlagTraits<vk::DebugUtilsMessageSeverityFlagBitsEXT>::allFlags)
        .setMessageType(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
            vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
            vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance)
        .setPfnUserCallback(&debug_callback);
    s_debug_messenger = instance.createDebugUtilsMessengerEXTUnique(debug_messenger_info);
}

void lcf::render::vkext::release_debug_utils_resources() noexcept
{
    s_debug_messenger.reset();
}
