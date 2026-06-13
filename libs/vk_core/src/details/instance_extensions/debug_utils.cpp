#include "vk_core/details/instance_extensions/debug_utils.h"
#include "log.h"
#include <format>

namespace {

vk::UniqueDebugUtilsMessengerEXT create_debug_utils_messenger(vk::Instance instance) noexcept;

} // namespace

namespace lcf::vkc::details {

ResourceLease enable_debug_utils(vk::Instance instance) noexcept
{
    auto debug_messenger = create_debug_utils_messenger(instance);
    if (not debug_messenger) { return {}; }
    auto debug_messenger_rp = make_resource_ptr<vk::UniqueDebugUtilsMessengerEXT>(std::move(debug_messenger));
    return debug_messenger_rp.lease();
}

} // namespace lcf::vkc::details

namespace {

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

vk::UniqueDebugUtilsMessengerEXT create_debug_utils_messenger(vk::Instance instance) noexcept
{
    if (not VULKAN_HPP_DEFAULT_DISPATCHER.vkCreateDebugUtilsMessengerEXT) {
        return {};
    }
    if (is_renderdoc_environment()) {
        lcf_log_warn("RenderDoc environment detected — skipping debug messenger creation");
        return {}; //! RenderDoc is contradictory to custom debug callback
    }
    vk::DebugUtilsMessengerCreateInfoEXT debug_messenger_info;
    debug_messenger_info.setMessageSeverity(vk::FlagTraits<vk::DebugUtilsMessageSeverityFlagBitsEXT>::allFlags)
        .setMessageType(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
            vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
            vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance)
        .setPfnUserCallback(&debug_callback);
    vk::UniqueDebugUtilsMessengerEXT debug_messenger {};
    try {
        debug_messenger = instance.createDebugUtilsMessengerEXTUnique(debug_messenger_info);
    } catch (const vk::SystemError & e) {
        lcf_log_error("Failed to create debug utils messenger: {}", e.what());
    }
    return debug_messenger;
}

} // namespace
