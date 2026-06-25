#include "vk_core/manifest/InstanceExtensionManifest.h"
#include "vk_core/manifest/DeviceExtensionManifest.h"
#include "vk_core/debug/entry.h"
#include "vk_core/debug/debug_utils.h"
#include "vk_core/WSI/entry.h"
#include "vk_core/WSI/WindowHandle.h"
#include "vk_core/WSI/Swapchain.h"
#include "vk_core/context/entry.h"
#include "vk_core/context/create_infos.h"
#include "vk_core/context/InstanceContext.h"
#include "vk_core/context/RenderDeviceContext.h"
#include "vk_core/context/QueueContext.h"
#include "vk_core/error.h"
#include <SDL3/SDL.h>
#include <atomic>
#include <thread>
#include <variant>
#include <array>
#include <optional>
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

vkc::wsi::WindowHandle to_wsi_window_handle(const WindowHandle & window_handle) noexcept;

//- delivered synchronously during a Win32 modal resize drag, where the main PollEvent loop is blocked.
bool on_window_event_watch(void * userdata, SDL_Event * event)
{
    if (event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
        static_cast<vkc::wsi::Swapchain *>(userdata)->resizeToFit();
    }
    return true;
}

// A 1x1 device-local image cleared to white, left in eTransferSrcOptimal so it
// can be used as the blit source for Swapchain::present.
struct WhiteImage
{
    vk::UniqueImage m_image;
    vk::UniqueDeviceMemory m_memory;
};

std::optional<WhiteImage> create_white_image(vkc::RenderDeviceContext & device_context, const std::array<float, 4> &color) noexcept;


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
    vkc::wsi::register_surface(inst_ext_manifest);
    vkc::wsi::register_swapchain(device_ext_manifest);

    vk::ApplicationInfo app_info;
    app_info.setPApplicationName("LCFEngine")
        .setPEngineName("LCFEngine")
        .setApplicationVersion(vk::makeVersion(1, 0, 0))
        .setEngineVersion(vk::makeVersion(1, 0, 0))
        .setApiVersion(vk::HeaderVersionComplete);
    
    vkc::InstanceContextCreateInfo instance_info; // same as bs::InstanceCreateInfo
    instance_info.setApplicationInfo(app_info)
        .addRequiredInstanceLayer("VK_LAYER_KHRONOS_validation")
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
    vkc::wsi::WindowHandle wsi_window_handle = to_wsi_window_handle(window_handle);
    vkc::wsi::Swapchain swapchain;
    if (auto ec = swapchain.create(instance_context.getInstance(), render_device_context, wsi_window_handle)) {
        lcf_log_error("Failed to create swapchain: {}", ec.message());
        return 1;
    }
    lcf_log_info("Swapchain created successfully.");

    auto white_image_opt = create_white_image(render_device_context, {1.0f, 1.0f, 1.0f, 1.0f});
    auto black_image_opt = create_white_image(render_device_context, {1.0f, 0.0f, 0.0f, 1.0f});
    if (not white_image_opt or not black_image_opt) {
        lcf_log_error("Failed to create white source image.");
        return 1;
    }
    vk::Image white_image = white_image_opt->m_image.get();
    vk::Image black_image = black_image_opt->m_image.get();
    std::array<vk::Image, 2> images = {white_image, black_image};
    const std::array<vk::Offset3D, 2> src_offsets {{ {0, 0, 0}, {1, 1, 1} }};

    //- resize during a Win32 modal drag is only delivered through an event watch (the main
    //- PollEvent loop is blocked). resize() replays the cached frame at the new size synchronously,
    //- so the new content reaches DWM on the same tick the OS grows the window (no black edge).
    //- swapchain serializes resize() and the render thread's present() via its internal mutex.
    SDL_AddEventWatch(on_window_event_watch, &swapchain);

    std::atomic<bool> running {true};
    std::thread render_thread([&] {
        static uint64_t frame = 0;
        while (running.load(std::memory_order_relaxed)) {
            auto ec = swapchain.present(src_offsets, images[frame++ % 2]);
            if (not ec or ec == vkc::errc::surface_zero_size or ec == vkc::errc::present_skipped_for_resize) { continue; }
            lcf_log_error("present failed: {}", ec.message());
        }
    });

    while (running.load(std::memory_order_relaxed)) {
        for (SDL_Event event; SDL_PollEvent(&event);) {
            switch (event.type) {
                case SDL_EVENT_QUIT: { running.store(false, std::memory_order_relaxed); } break;
                default: break;
            }
        }
    }

    render_thread.join();
    SDL_RemoveEventWatch(on_window_event_watch, &swapchain);
    render_device_context.getDevice().waitIdle();
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

vkc::wsi::WindowHandle to_wsi_window_handle(const WindowHandle & window_handle) noexcept
{
    return std::visit([](const auto & handle) -> vkc::wsi::WindowHandle {
        using T = std::decay_t<decltype(handle)>;
        if constexpr (std::is_same_v<T, win32::WindowHandle>) {
            return vkc::wsi::win32::WindowHandle(handle.m_hinstance, handle.m_hwnd);
        } else if constexpr (std::is_same_v<T, xcb::WindowHandle>) {
            return vkc::wsi::xcb::WindowHandle(handle.m_connection, handle.m_window);
        } else if constexpr (std::is_same_v<T, wayland::WindowHandle>) {
            return vkc::wsi::wayland::WindowHandle(handle.m_display, handle.m_surface);
        } else {
            return vkc::wsi::metal::WindowHandle(handle.m_layer);
        }
    }, window_handle);
}

std::optional<WhiteImage> create_white_image(vkc::RenderDeviceContext & device_context, const std::array<float, 4> & color) noexcept
{
    vk::Device device = device_context.getDevice();
    vk::PhysicalDevice physical_device = device_context.getPhysicalDevice();
    uint32_t gfx_family = device_context.getGraphicsQueueContext().getFamilyIndex();
    vk::Queue gfx_queue = device_context.getGraphicsQueueContext().getQueue();

    WhiteImage white;
    try {
        vk::ImageCreateInfo image_info;
        image_info.setImageType(vk::ImageType::e2D)
            .setFormat(vk::Format::eR8G8B8A8Unorm)
            .setExtent({1u, 1u, 1u})
            .setMipLevels(1u)
            .setArrayLayers(1u)
            .setSamples(vk::SampleCountFlagBits::e1)
            .setTiling(vk::ImageTiling::eOptimal)
            .setUsage(vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc)
            .setInitialLayout(vk::ImageLayout::eUndefined);
        white.m_image = device.createImageUnique(image_info);

        vk::MemoryRequirements req = device.getImageMemoryRequirements(white.m_image.get());
        vk::PhysicalDeviceMemoryProperties mem_props = physical_device.getMemoryProperties();
        uint32_t mem_type_index = UINT32_MAX;
        for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
            bool type_ok = req.memoryTypeBits & (1u << i);
            bool device_local = bool(mem_props.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal);
            if (type_ok and device_local) { mem_type_index = i; break; }
        }
        if (mem_type_index == UINT32_MAX) {
            lcf_log_error("No device-local memory type for white image.");
            return std::nullopt;
        }
        white.m_memory = device.allocateMemoryUnique({req.size, mem_type_index});
        device.bindImageMemory(white.m_image.get(), white.m_memory.get(), 0);
        // one-time submit: undefined -> transferDst, clear white, transferDst -> transferSrc
        vk::UniqueCommandPool pool = device.createCommandPoolUnique({vk::CommandPoolCreateFlagBits::eTransient, gfx_family});
        vk::CommandBuffer cmd = device.allocateCommandBuffers({pool.get(), vk::CommandBufferLevel::ePrimary, 1u}).front();

        vk::ImageSubresourceRange range {vk::ImageAspectFlagBits::eColor, 0u, 1u, 0u, 1u};
        vk::ImageMemoryBarrier to_dst, to_src;
        to_dst.setImage(white.m_image.get())
            .setOldLayout(vk::ImageLayout::eUndefined)
            .setNewLayout(vk::ImageLayout::eTransferDstOptimal)
            .setSubresourceRange(range)
            .setSrcAccessMask(vk::AccessFlagBits::eNone)
            .setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
        to_src.setImage(white.m_image.get())
            .setOldLayout(vk::ImageLayout::eTransferDstOptimal)
            .setNewLayout(vk::ImageLayout::eTransferSrcOptimal)
            .setSubresourceRange(range)
            .setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
            .setDstAccessMask(vk::AccessFlagBits::eTransferRead);

        vk::ClearColorValue white_color {color};
        cmd.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
        cmd.pipelineBarrier(
            vk::PipelineStageFlagBits::eTopOfPipe,
            vk::PipelineStageFlagBits::eTransfer,
            {}, {}, {}, to_dst);
        cmd.clearColorImage(white.m_image.get(), vk::ImageLayout::eTransferDstOptimal, white_color, range);
        cmd.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eTransfer,
            {}, {}, {}, to_src);
        cmd.end();

        vk::UniqueFence fence = device.createFenceUnique({});
        gfx_queue.submit(vk::SubmitInfo().setCommandBuffers(cmd), fence.get());
        auto wait_result = device.waitForFences(fence.get(), VK_TRUE, UINT64_MAX);
        if (wait_result != vk::Result::eSuccess) {
            lcf_log_error("waitForFences failed while preparing white image.");
            return std::nullopt;
        }
    } catch (const vk::SystemError & e) {
        lcf_log_error("create_white_image failed: {}", e.what());
        return std::nullopt;
    }
    return white;
}