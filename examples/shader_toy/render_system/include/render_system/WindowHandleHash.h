#pragma once

#include "vk_core/WSI/WindowHandle.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <variant>

namespace lcf::shader_toy {

namespace details {

inline void hash_combine(std::size_t & seed, std::size_t value) noexcept
{
    seed ^= value + static_cast<std::size_t>(0x9e3779b97f4a7c15ull) + (seed << 6u) + (seed >> 2u);
}

inline std::size_t hash_pointer(const void * pointer) noexcept
{
    return std::hash<std::uintptr_t> {}(reinterpret_cast<std::uintptr_t>(pointer));
}

} // namespace details

struct WindowHandleHash
{
    std::size_t operator()(const vkc::wsi::WindowHandle & handle) const noexcept
    {
        std::size_t seed = std::hash<std::size_t> {}(handle.index());
        std::visit([&seed](const auto & value) noexcept {
            using Handle = std::remove_cvref_t<decltype(value)>;
            if constexpr (std::is_same_v<Handle, vkc::wsi::win32::WindowHandle>) {
                details::hash_combine(seed, details::hash_pointer(value.m_hinstance));
                details::hash_combine(seed, details::hash_pointer(value.m_hwnd));
            } else if constexpr (std::is_same_v<Handle, vkc::wsi::xcb::WindowHandle>) {
                details::hash_combine(seed, details::hash_pointer(value.m_connection));
                details::hash_combine(seed, std::hash<uint32_t> {}(value.m_window));
            } else if constexpr (std::is_same_v<Handle, vkc::wsi::xlib::WindowHandle>) {
                details::hash_combine(seed, details::hash_pointer(value.m_display));
                details::hash_combine(seed, std::hash<uint64_t> {}(value.m_window));
            } else if constexpr (std::is_same_v<Handle, vkc::wsi::wayland::WindowHandle>) {
                details::hash_combine(seed, details::hash_pointer(value.m_display));
                details::hash_combine(seed, details::hash_pointer(value.m_surface));
            } else if constexpr (std::is_same_v<Handle, vkc::wsi::metal::WindowHandle>) {
                details::hash_combine(seed, details::hash_pointer(value.m_layer));
            }
        }, handle);
        return seed;
    }
};

struct WindowHandleEqual
{
    bool operator()(const vkc::wsi::WindowHandle & lhs, const vkc::wsi::WindowHandle & rhs) const noexcept
    {
        if (lhs.index() != rhs.index()) { return false; }
        return std::visit([](const auto & left, const auto & right) noexcept {
            using Left = std::remove_cvref_t<decltype(left)>;
            using Right = std::remove_cvref_t<decltype(right)>;
            if constexpr (not std::is_same_v<Left, Right>) {
                return false;
            } else if constexpr (std::is_same_v<Left, vkc::wsi::win32::WindowHandle>) {
                return left.m_hinstance == right.m_hinstance && left.m_hwnd == right.m_hwnd;
            } else if constexpr (std::is_same_v<Left, vkc::wsi::xcb::WindowHandle>) {
                return left.m_connection == right.m_connection && left.m_window == right.m_window;
            } else if constexpr (std::is_same_v<Left, vkc::wsi::xlib::WindowHandle>) {
                return left.m_display == right.m_display && left.m_window == right.m_window;
            } else if constexpr (std::is_same_v<Left, vkc::wsi::wayland::WindowHandle>) {
                return left.m_display == right.m_display && left.m_surface == right.m_surface;
            } else {
                return left.m_layer == right.m_layer;
            }
        }, lhs, rhs);
    }
};

} // namespace lcf::shader_toy
