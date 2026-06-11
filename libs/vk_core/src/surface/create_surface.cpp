#include "vk_core/surface/create_surface.h"

namespace lcf::vkc::surf {

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

vk::UniqueSurfaceKHR create_surface( vk::Instance instance, const WindowHandle & window_handle, const vk::AllocationCallbacks * allocator)
{
    return std::visit( [&]<typename Handle>(const Handle & handle) {
        if constexpr (std::same_as<Handle, win32::WindowHandle>) { return win32::create_surface(instance, handle, allocator); }
        if constexpr (std::same_as<Handle, xcb::WindowHandle>) { return xcb::create_surface(instance, handle, allocator); }
        if constexpr (std::same_as<Handle, wayland::WindowHandle>) { return wayland::create_surface(instance, handle, allocator); }
        if constexpr (std::same_as<Handle, metal::WindowHandle>) { return metal::create_surface(instance, handle, allocator); }
        return vk::UniqueSurfaceKHR{};
    }, window_handle);
}

} // namespace lcf::vkc::surf
