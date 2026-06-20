#include "vk_core/manifest/InstanceExtensionManifest.h"
#include "vk_core/manifest/DeviceExtensionManifest.h"
#include "vk_core/debug/entry.h"
#include "vk_core/debug/debug_utils.h"
#include "vk_core/surface/entry.h"
#include "vk_core/context/entry.h"
#include "vk_core/context/create_infos.h"
#include "vk_core/context/InstanceContext.h"
#include "vk_core/context/RenderDeviceContext.h"
#include "vk_core/context/QueueContext.h"
#include <SDL3/SDL.h>
#include <atomic>
#include "log.h"

using namespace lcf;
namespace stdv = std::views;

int main()
{
    log::init();
    vkc::InstanceExtensionManifest inst_ext_manifest;
    vkc::DeviceExtensionManifest device_ext_manifest;

    vkc::register_context_module(inst_ext_manifest, device_ext_manifest);
    vkc::dbg::DebugLogCallbacks debug_callbacks;
    debug_callbacks.setWarningSink([](std::string_view message) { lcf_log_warn(message); })
        .setErrorSink([](std::string_view message) { lcf_log_error(message); });
    vkc::dbg::register_debug_utils(inst_ext_manifest, vkc::dbg::SeverityFlags::eError | vkc::dbg::SeverityFlags::eWarning, debug_callbacks);
    vkc::surf::register_surface(inst_ext_manifest);
    vkc::surf::register_swapchain(device_ext_manifest);

    vk::ApplicationInfo app_info;
    app_info.setPApplicationName("LCFEngine")
        .setPEngineName("LCFEngine")
        .setApplicationVersion(vk::makeVersion(1, 0, 0))
        .setEngineVersion(vk::makeVersion(1, 0, 0))
        .setApiVersion(vk::HeaderVersionComplete);
    
    vkc::InstanceContextCreateInfo instance_info; // same as bs::InstanceCreateInfo
    instance_info.setApplicationInfo(app_info)
        .setRequiredInstanceExtensionManifest(inst_ext_manifest);

    vkc::bs::PhysicalDeviceSelectInfo physical_device_select_info;
    physical_device_select_info.setRequiredDeviceExtensionManifest(device_ext_manifest)
        .setPreferredType(vk::PhysicalDeviceType::eDiscreteGpu);
    vkc::DeviceContextCreateInfo device_context_info;
    device_context_info.setRequiredDeviceExtensionManifest(device_ext_manifest)
        .setPhysicalDeviceSelectInfo(physical_device_select_info);
    
    vkc::InstanceContext instance_context;
    if (auto ec = instance_context.create(instance_info)) {
        lcf_log_error("Failed to create instance_context: {}", ec.message());
        return 1;
    }
    lcf_log_info("InstanceContext created successfully.");

    vkc::RenderDeviceContext render_device_context;
    if (auto ec = render_device_context.create(instance_context.getInstance(), device_context_info)) {
        lcf_log_error("Failed to create render_device_context: {}", ec.message());
        return 1;
    }

    SDL_Init(SDL_INIT_VIDEO);
    uint32_t width = 800, height = 600;
    if (const SDL_DisplayMode * mode = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay())) {
        width = static_cast<uint32_t>(mode->w) / 2;
        height = static_cast<uint32_t>(mode->h) / 2;
    }
    SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE  | SDL_WINDOW_VULKAN;
    
    auto window_p = SDL_CreateWindow("hello swapchain", width, height, window_flags);
    if (not window_p) {
        lcf_log_error("Failed to create SDL window: {}", SDL_GetError());
        return 1;
    } 
    SDL_ShowWindow(window_p);
    std::atomic<bool> running {true};
    while (running.load(std::memory_order_relaxed)) {
        for (SDL_Event event; SDL_PollEvent(&event);) {
            switch (event.type) {
                case SDL_EVENT_QUIT: { running.store(false, std::memory_order_relaxed); } break;
                default: break;
            }
        }
    }
    
    SDL_DestroyWindow(window_p);
    SDL_Quit();
    return 0;
}
