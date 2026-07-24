#include "vk_core/WSI/create_surface.h"
#include "vk_core/WSI/entry.h"
#include "vk_core/manifest/InstanceExtensionManifest.h"

namespace lcf::vkc::entry {

void register_surface(InstanceExtensionManifest & manifest) noexcept
{
    static constexpr std::array k_extensions
    {
        vk::KHRSurfaceExtensionName,
    #if defined(VK_USE_PLATFORM_WIN32_KHR)
        vk::KHRWin32SurfaceExtensionName,
    #endif
    #if defined(VK_USE_PLATFORM_XCB_KHR)
        vk::KHRXcbSurfaceExtensionName,
    #endif
    #if defined(VK_USE_PLATFORM_XLIB_KHR)
        vk::KHRXlibSurfaceExtensionName,
    #endif
    #if defined(VK_USE_PLATFORM_WAYLAND_KHR)
        vk::KHRWaylandSurfaceExtensionName,
    #endif
    #if defined(VK_USE_PLATFORM_METAL_EXT)
        vk::EXTMetalSurfaceExtensionName,
    #endif
    };
    manifest.addRequiredExtensions(k_extensions);
}

} // namespace lcf::vkc::entry

namespace {

using namespace lcf::vkc::wsi;

vk::UniqueSurfaceKHR create_surface_maythrow(vk::Instance instance, const WindowHandle & window_handle, const vk::AllocationCallbacks * allocator);

} // anoymous namespace

namespace lcf::vkc::wsi {

namespace win32 {

vk::UniqueSurfaceKHR create_surface(
    vk::Instance instance, const WindowHandle & handle, const vk::AllocationCallbacks * allocator)
{
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    vk::Win32SurfaceCreateInfoKHR create_info;
    create_info.setHinstance(static_cast<HINSTANCE>(handle.m_hinstance))
        .setHwnd(static_cast<HWND>(handle.m_hwnd));
    return instance.createWin32SurfaceKHRUnique(create_info, allocator);
#else
    return {};
#endif // VK_USE_PLATFORM_WIN32_KHR
}

} // namespace win32

namespace xcb {

vk::UniqueSurfaceKHR create_surface( vk::Instance instance, const WindowHandle & handle, const vk::AllocationCallbacks * allocator)
{
#if defined(VK_USE_PLATFORM_XCB_KHR)
    vk::XcbSurfaceCreateInfoKHR create_info;
    create_info.setConnection(static_cast<xcb_connection_t *>(handle.m_connection))
        .setWindow(static_cast<xcb_window_t>(handle.m_window));
    return instance.createXcbSurfaceKHRUnique(create_info, allocator);
#else
    return {};
#endif // VK_USE_PLATFORM_XCB_KHR
}

} // namespace xcb

namespace xlib {

vk::UniqueSurfaceKHR create_surface(
    vk::Instance instance, const WindowHandle & handle, const vk::AllocationCallbacks * allocator)
{
#if defined(VK_USE_PLATFORM_XLIB_KHR)
    vk::XlibSurfaceCreateInfoKHR create_info;
    create_info.setDpy(static_cast<Display *>(handle.m_display))
        .setWindow(static_cast<Window>(handle.m_window));
    return instance.createXlibSurfaceKHRUnique(create_info, allocator);
#else
    return {};
#endif // VK_USE_PLATFORM_XLIB_KHR
}

} // namespace xlib

namespace wayland {

vk::UniqueSurfaceKHR create_surface(
    vk::Instance instance, const WindowHandle & handle, const vk::AllocationCallbacks * allocator)
{
#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
    vk::WaylandSurfaceCreateInfoKHR create_info;
    create_info.setDisplay(static_cast<wl_display *>(handle.m_display))
        .setSurface(static_cast<wl_surface *>(handle.m_surface));
    return instance.createWaylandSurfaceKHRUnique(create_info, allocator);
#else
    return {};
#endif // VK_USE_PLATFORM_WAYLAND_KHR
}

} // namespace wayland

namespace metal {

vk::UniqueSurfaceKHR create_surface(vk::Instance instance, const WindowHandle & handle, const vk::AllocationCallbacks * allocator)
{
#if defined(VK_USE_PLATFORM_METAL_EXT)
    vk::MetalSurfaceCreateInfoEXT create_info;
    create_info.setPLayer(static_cast<const CAMetalLayer *>(handle.m_layer));
    return instance.createMetalSurfaceEXTUnique(create_info, allocator);
#else
    return {};
#endif // VK_USE_PLATFORM_METAL_EXT
}

} // namespace metal


std::expected<vk::UniqueSurfaceKHR, std::error_code> create_surface(vk::Instance instance, const WindowHandle & window_handle, const vk::AllocationCallbacks * allocator) noexcept
{
    try {
        return create_surface_maythrow(instance, window_handle, allocator);
    } catch (vk::SystemError & e) {
        return std::unexpected {e.code()};
    }
    return {};
}

} // namespace lcf::vkc::wsi

namespace {

vk::UniqueSurfaceKHR create_surface_maythrow(vk::Instance instance, const WindowHandle & window_handle, const vk::AllocationCallbacks * allocator)
{
    return std::visit( [&]<typename Handle>(const Handle & handle) {
        if constexpr (std::same_as<Handle, win32::WindowHandle>) { return win32::create_surface(instance, handle, allocator); }
        if constexpr (std::same_as<Handle, xcb::WindowHandle>) { return xcb::create_surface(instance, handle, allocator); }
        if constexpr (std::same_as<Handle, xlib::WindowHandle>) { return xlib::create_surface(instance, handle, allocator); }
        if constexpr (std::same_as<Handle, wayland::WindowHandle>) { return wayland::create_surface(instance, handle, allocator); }
        if constexpr (std::same_as<Handle, metal::WindowHandle>) { return metal::create_surface(instance, handle, allocator); }
        return vk::UniqueSurfaceKHR{};
    }, window_handle);
}

} // anoymous namespace
