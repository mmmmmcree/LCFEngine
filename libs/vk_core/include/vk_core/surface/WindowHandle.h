#pragma once

#include <cstdint>
#include <variant>

namespace lcf::vkc::surf {

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
    WindowHandle(const void * layer) : m_layer(layer) {}

    const void * m_layer;
};

} // namespace metal

using WindowHandle = std::variant<
    win32::WindowHandle,
    xcb::WindowHandle,
    wayland::WindowHandle,
    metal::WindowHandle>;

} // namespace lcf::vkc::surf