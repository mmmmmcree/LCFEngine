#include "vk_core/manifest/InstanceExtensionManifest.h"
#include "vk_core/manifest/DeviceExtensionManifest.h"
#include "vk_core/debug/entry.h"
#include "vk_core/debug/debug_utils.h"
#include "vk_core/surface/entry.h"
#include "vk_core/surface/WindowHandle.h"
#include "vk_core/surface/create_surface.h"
#include "vk_core/context/entry.h"
#include "vk_core/context/create_infos.h"
#include "vk_core/context/InstanceContext.h"
#include "vk_core/context/RenderDeviceContext.h"
#include "vk_core/context/QueueContext.h"
#include <SDL3/SDL.h>
#include <atomic>
#include <variant>
#include "log.h"

using namespace lcf;
namespace stdv = std::views;

namespace win32 {

struct WindowHandle
{
    WindowHandle(void * hinstance, void * hwnd) : m_hinstance(hinstance), m_hwnd(hwnd) {}

    void * m_hinstance;
    void * m_hwnd;
};

} // namespace win32

namespace xcb {

struct WindowHandle
{
    WindowHandle(void * connection, uint32_t window) : m_connection(connection), m_window(window) {}

    void * m_connection;
    uint32_t m_window;
};


} // namespace xlib

namespace wayland {

struct WindowHandle
{
    WindowHandle(void * display, void * surface) : m_display(display), m_surface(surface) {}

    void * m_display;
    void * m_surface;
};

} // namespace wayland

namespace metal {

struct WindowHandle
{
    WindowHandle(void * layer) : m_layer(layer) {}

    const void * m_layer;
};

} // namespace metal

using WindowHandle = std::variant<
    win32::WindowHandle,
    xcb::WindowHandle,
    wayland::WindowHandle,
    metal::WindowHandle>;

WindowHandle make_window_handle(SDL_Window * window_p) noexcept;

vkc::surf::WindowHandle to_surf_window_handle(const WindowHandle & window_handle) noexcept;


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
    
    SDL_Window * window_p = SDL_CreateWindow("hello swapchain", width, height, window_flags);
    if (not window_p) {
        lcf_log_error("Failed to create SDL window: {}", SDL_GetError());
        return 1;
    } 
    
    SDL_ShowWindow(window_p);
    WindowHandle window_handle = make_window_handle(window_p);
    vk::UniqueSurfaceKHR surface = vkc::surf::create_surface(
        instance_context.getInstance(), to_surf_window_handle(window_handle));
    if (not surface) {
        lcf_log_error("Failed to create surface.");
        return 1;
    }
    lcf_log_info("Surface created successfully.");
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

WindowHandle make_window_handle(SDL_Window * window_p) noexcept
{
    const SDL_PropertiesID props = SDL_GetWindowProperties(window_p);
#if defined(SDL_PLATFORM_WIN32)
    return win32::WindowHandle(
        SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_INSTANCE_POINTER, nullptr),
        SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
#elif defined(SDL_PLATFORM_APPLE)
    return metal::WindowHandle(SDL_Metal_GetLayer(SDL_Metal_CreateView(window_p)));
#else
    if (std::string_view(SDL_GetCurrentVideoDriver()) == "wayland") {
        return wayland::WindowHandle(
            SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr),
            SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr));
    }
    return xcb::WindowHandle(
        SDL_GetPointerProperty(props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr),
        static_cast<uint32_t>(SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0)));
#endif
}

vkc::surf::WindowHandle to_surf_window_handle(const WindowHandle & window_handle) noexcept
{
    return std::visit([](const auto & handle) -> vkc::surf::WindowHandle {
        using T = std::decay_t<decltype(handle)>;
        if constexpr (std::is_same_v<T, win32::WindowHandle>) {
            return vkc::surf::win32::WindowHandle(handle.m_hinstance, handle.m_hwnd);
        } else if constexpr (std::is_same_v<T, xcb::WindowHandle>) {
            return vkc::surf::xcb::WindowHandle(handle.m_connection, handle.m_window);
        } else if constexpr (std::is_same_v<T, wayland::WindowHandle>) {
            return vkc::surf::wayland::WindowHandle(handle.m_display, handle.m_surface);
        } else {
            return vkc::surf::metal::WindowHandle(handle.m_layer);
        }
    }, window_handle);
}