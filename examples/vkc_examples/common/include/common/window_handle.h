#pragma once

#include "vk_core/WSI/WindowHandle.h"
#include "win/Window.h"

#include <type_traits>
#include <variant>

namespace vkce {

inline lcf::vkc::wsi::WindowHandle to_wsi_window_handle(
    const lcf::win::WindowHandle & window_handle) noexcept
{
    namespace vkc = lcf::vkc;
    namespace win = lcf::win;

    return std::visit([](const auto & handle) -> vkc::wsi::WindowHandle {
        using T = std::decay_t<decltype(handle)>;

        if constexpr (std::is_same_v<T, win::win32::WindowHandle>) {
            return vkc::wsi::win32::WindowHandle(handle.m_hinstance, handle.m_hwnd);
        } else if constexpr (std::is_same_v<T, win::xcb::WindowHandle>) {
            return vkc::wsi::xcb::WindowHandle(handle.m_connection, handle.m_window);
        } else if constexpr (std::is_same_v<T, win::xlib::WindowHandle>) {
            return vkc::wsi::xlib::WindowHandle(handle.m_display, handle.m_window);
        } else if constexpr (std::is_same_v<T, win::wayland::WindowHandle>) {
            return vkc::wsi::wayland::WindowHandle(handle.m_display, handle.m_surface);
        } else {
            return vkc::wsi::metal::WindowHandle(handle.m_layer);
        }
    }, window_handle);
}

} // namespace vkce
