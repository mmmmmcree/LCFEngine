#include "vk_core/debug/debug_utils.h"
#include "vk_core/debug/entry.h"
#include "vk_core/manifest/InstanceExtensionManifest.h"
#include <array>
#include <format>
#include <iostream>


namespace {

struct DebugUtilsContext
{
    lcf::vkc::dbg::DebugLogCallbacks callbacks;
    vk::DebugUtilsMessageSeverityFlagsEXT severity;
    vk::UniqueDebugUtilsMessengerEXT messenger;
};

vk::UniqueDebugUtilsMessengerEXT create_debug_utils_messenger(vk::Instance instance, void * user_data) noexcept;

bool is_renderdoc_environment() noexcept;

} // namespace

namespace lcf::vkc::dbg {

ResourceLease enable_debug_utils(
    vk::Instance instance,
    vk::DebugUtilsMessageSeverityFlagsEXT severity,
    DebugLogCallbacks callbacks) noexcept
{
    auto context_rp = make_resource_ptr<DebugUtilsContext>(std::move(callbacks), severity, vk::UniqueDebugUtilsMessengerEXT{});
    context_rp->messenger = create_debug_utils_messenger(instance, context_rp.get());
    if (not context_rp->messenger) { return {}; }
    return context_rp.lease();
}


void register_debug_utils(
    InstanceExtensionManifest & manifest,
    vk::DebugUtilsMessageSeverityFlagsEXT severity,
    const DebugLogCallbacks & callbacks) noexcept
{
    static constexpr std::array s_ext_names { vk::EXTDebugUtilsExtensionName };
    if (is_renderdoc_environment()) {
        return; //! RenderDoc is contradictory to this extension
    }
    manifest.addRequiredExtensions(s_ext_names);
    manifest.addExtensionEnableCallback([severity, callbacks](vk::Instance instance) {
        return enable_debug_utils(instance, severity, callbacks);
    });
}

void register_debug_utils(InstanceExtensionManifest & manifest) noexcept
{
    return register_debug_utils(manifest, SeverityFlags::eWarning | SeverityFlags::eError, {});
}

DebugLogCallbacks::DebugLogCallbacks() noexcept :
    m_verbose_log_sink([](std::string_view message) { std::cerr << "verbose: " << message << '\n'; }),
    m_info_log_sink([](std::string_view message) { std::cerr << "info: " << message << '\n'; }),
    m_warning_log_sink([](std::string_view message) { std::cerr << "warning: " << message << '\n'; }),
    m_error_log_sink([](std::string_view message) { std::cerr << "error: " << message << '\n'; })
{}

} // namespace lcf::vkc::dbg

namespace {

static VKAPI_ATTR vk::Bool32 VKAPI_CALL debug_callback(
    vk::DebugUtilsMessageSeverityFlagBitsEXT severity_flags,
    vk::DebugUtilsMessageTypeFlagsEXT type_flags,
    const vk::DebugUtilsMessengerCallbackDataEXT * callback_data,
    void * user_data)
{
    if (not user_data or not callback_data->pMessage) { return false; }
    const auto * context = static_cast<DebugUtilsContext *>(user_data);
    if (not (severity_flags & context->severity)) { return false; }
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
    using SeverityFlags = vk::DebugUtilsMessageSeverityFlagBitsEXT;
    const auto & callbacks = context->callbacks;
    if (severity_flags & SeverityFlags::eError) { callbacks.logError(log_output); }
    else if (severity_flags & SeverityFlags::eWarning) { callbacks.logWarning(log_output); }
    else if (severity_flags & SeverityFlags::eInfo) { callbacks.logInfo(log_output); }
    else if (severity_flags & SeverityFlags::eVerbose) { callbacks.logVerbose(log_output); }
    return false;
}

bool is_renderdoc_environment() noexcept
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

vk::UniqueDebugUtilsMessengerEXT create_debug_utils_messenger(vk::Instance instance, void * user_data) noexcept
{
    if (not VULKAN_HPP_DEFAULT_DISPATCHER.vkCreateDebugUtilsMessengerEXT) {
        return {};
    }
    
    vk::DebugUtilsMessengerCreateInfoEXT debug_messenger_info;
    debug_messenger_info.setMessageSeverity(vk::FlagTraits<vk::DebugUtilsMessageSeverityFlagBitsEXT>::allFlags)
        .setMessageType(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
            vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
            vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance)
        .setPfnUserCallback(&debug_callback)
        .setPUserData(user_data);
    vk::UniqueDebugUtilsMessengerEXT debug_messenger {};
    try {
        debug_messenger = instance.createDebugUtilsMessengerEXTUnique(debug_messenger_info);
    } catch (const vk::SystemError &) {
        return {};
    }
    return debug_messenger;
}

} // namespace
