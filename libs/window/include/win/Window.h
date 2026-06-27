#pragma once

#include <memory>
#include <functional>
#include <system_error>
#include <span>
#include <string>
#include <utility>
#include "WindowHandle.h"
#include "WindowEvent.h"
#include "enums.h"

namespace lcf::win {

class WindowCreateInfo;

class Window
{
    using Self = Window;
    struct Impl;
public:
    ~Window() noexcept;
    Window() noexcept;
    Window(const Self &) = delete;
    Window(Self &&) noexcept;
    Self &operator=(const Self &) = delete;
    Self &operator=(Self &&) noexcept;
public:
    std::error_code create(const WindowCreateInfo & window_info) noexcept;
    std::error_code show() const noexcept;
    WindowHandle handle() const noexcept;
    std::pair<uint32_t, uint32_t> getPixelSize() const noexcept;

    //- Vulkan cmd-buffer style: drains the backend queue into a reused internal
    //- buffer and returns a view over it, valid until the next pollEvents().
    //- Resize is NOT in this stream (see setResizeCallback).
    std::span<const WindowEvent> pollEvents() const noexcept;
    void setResizeCallback(std::function<void(const ResizeEvent &)> callback) noexcept;
private:
    std::unique_ptr<Impl> m_impl_up;
};

class WindowCreateInfo
{
    using Self = WindowCreateInfo;
public:
    WindowCreateInfo(
        std::string_view title = "",
        float width_ratio = 0.5f,
        float height_ratio = 0.5f,
        WindowFlags flags = WindowFlags::eResizable
    ) noexcept :
        m_title(title),
        m_width_ratio(width_ratio),
        m_height_ratio(height_ratio),
        m_flags(flags)
    {}
public:
    Self & setTitle(std::string_view title) { m_title = title; return *this; }
    Self & setWidthRatio(float ratio) { m_width_ratio = ratio; return *this; }
    Self & setHeightRatio(float ratio) { m_height_ratio = ratio; return *this; }
    Self & addFlags(WindowFlags flags) { m_flags |= flags; return *this; }
    const std::string & getTitle() const { return m_title; }
    float getWidthRatio() const { return m_width_ratio; }
    float getHeightRatio() const { return m_height_ratio; }
    WindowFlags getFlags() const { return m_flags; }
private:
    std::string m_title;
    float m_width_ratio;
    float m_height_ratio;
    WindowFlags m_flags;
};

} // namespace lcf::win
