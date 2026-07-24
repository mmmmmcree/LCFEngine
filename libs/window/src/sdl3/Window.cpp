#include "win/Window.h"
#include "win/error.h"
#include <SDL3/SDL.h>
#include <atomic>
#include <vector>
#include <span>
#include <string_view>

namespace {

using namespace lcf::win;

static std::atomic<int> s_init_refcount {0};

bool initialize() noexcept;

void quit() noexcept;

SDL_WindowFlags to_sdl_window_flags(lcf::win::WindowFlags window_flags) noexcept;

} // anonymous namespace

namespace lcf::win {

struct Window::Impl
{
    static bool event_watch(void * userdata, SDL_Event * event);

    ~Impl();

    std::error_code create(const WindowCreateInfo & window_info) noexcept;
    std::error_code show() const noexcept;
    WindowHandle handle() const noexcept;
    std::pair<uint32_t, uint32_t> getPixelSize() const noexcept;
    std::span<const WindowEvent> pollEvents() noexcept;
    void setResizeCallback(std::function<void(const ResizeEvent &)> callback) noexcept;

    SDL_Window * m_window = nullptr;
    SDL_WindowID m_window_id = 0;
    bool m_is_initialized = false;
    std::vector<WindowEvent> m_events;
    std::function<void(const ResizeEvent &)> m_resize_callback = [](const ResizeEvent &) {};
};

bool Window::Impl::event_watch(void * userdata, SDL_Event * event)
{
    auto * self = static_cast<Impl *>(userdata);
    switch (event->type) {
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
            if (event->window.windowID != self->m_window_id) { return false; }
            ResizeEvent resize_event { static_cast<uint32_t>(event->window.data1), static_cast<uint32_t>(event->window.data2) };
            self->m_resize_callback(resize_event);
        } break;
        default: break;
    }
    return true;
}

Window::Impl::~Impl()
{
    if (m_window) {
        SDL_RemoveEventWatch(&Impl::event_watch, this);
        SDL_DestroyWindow(m_window);
    }
    if (m_is_initialized) {
        quit();
    }
}

std::error_code Window::Impl::create(const WindowCreateInfo & window_info) noexcept
{
    if (not initialize()) { return make_error_code(errc::backend_init_failed); }
    m_is_initialized = true;
    const SDL_DisplayMode * mode = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay());
    if (not mode) { return make_error_code(errc::display_query_failed); }
    const int32_t width = static_cast<int32_t>(static_cast<float>(mode->w) * window_info.getWidthRatio());
    const int32_t height = static_cast<int32_t>(static_cast<float>(mode->h) * window_info.getHeightRatio());

    const SDL_WindowFlags flags = to_sdl_window_flags(window_info.getFlags());
    m_window = SDL_CreateWindow(
        window_info.getTitle().c_str(),
        width, height, flags);
    if (not m_window) { return make_error_code(errc::window_creation_failed); }
    m_window_id = SDL_GetWindowID(m_window);
    SDL_AddEventWatch(&Impl::event_watch, this);
    return {};
}

std::error_code Window::Impl::show() const noexcept
{
    SDL_ShowWindow(m_window);
    return {};
}

WindowHandle Window::Impl::handle() const noexcept
{
    const SDL_PropertiesID props = SDL_GetWindowProperties(m_window);
#if defined(SDL_PLATFORM_WIN32)
    return win32::WindowHandle(
        SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_INSTANCE_POINTER, nullptr),
        SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
#elif defined(SDL_PLATFORM_APPLE)
    return metal::WindowHandle(SDL_Metal_GetLayer(SDL_Metal_CreateView(m_window)));
#else
    if (std::string_view(SDL_GetCurrentVideoDriver()) == "wayland") {
        return wayland::WindowHandle(
            SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr),
            SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr));
    }
    return xlib::WindowHandle(
        SDL_GetPointerProperty(props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr),
        static_cast<uint64_t>(SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0)));
#endif
}

std::pair<uint32_t, uint32_t> Window::Impl::getPixelSize() const noexcept
{
    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(m_window, &w, &h);
    return { static_cast<uint32_t>(w), static_cast<uint32_t>(h) };
}

std::span<const WindowEvent> Window::Impl::pollEvents() noexcept
{
    m_events.clear();
    for (SDL_Event event; SDL_PollEvent(&event);) {
        switch (event.type) {
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
                if (event.window.windowID != m_window_id) { break; }
                m_events.emplace_back(CloseEvent {});
            } break;
            case SDL_EVENT_QUIT: {
                m_events.emplace_back(CloseEvent {});
            } break;
            default: break;
        }
    }
    return m_events;
}

void Window::Impl::setResizeCallback(std::function<void(const ResizeEvent &)> callback) noexcept
{
    m_resize_callback = std::move(callback);
}

} // namespace lcf::win

namespace {

bool initialize() noexcept
{
    if (s_init_refcount.fetch_add(1, std::memory_order_acq_rel) > 0) { return true; }
    if (not SDL_Init(SDL_INIT_VIDEO)) {
        s_init_refcount.fetch_sub(1, std::memory_order_acq_rel);
        return false;
    }
    return true;
}

void quit() noexcept
{
    if (s_init_refcount.fetch_sub(1, std::memory_order_acq_rel) != 1) { return; }
    SDL_Quit();
}

SDL_WindowFlags to_sdl_window_flags(lcf::win::WindowFlags window_flags) noexcept
{
    constexpr std::pair<WindowFlags, SDL_WindowFlags> table[] = {
        { WindowFlags::eResizable,  SDL_WINDOW_RESIZABLE },
        { WindowFlags::eBorderless, SDL_WINDOW_BORDERLESS },
        { WindowFlags::eHidden,     SDL_WINDOW_HIDDEN },
        { WindowFlags::eFullscreen, SDL_WINDOW_FULLSCREEN },
    };
    SDL_WindowFlags flags = 0;
    for (const auto & [bit, sdl_bit] : table) {
        if (lcf::contains_flags(window_flags, bit)) { flags |= sdl_bit; }
    }
    return flags;
}

} // anonymous namespace

namespace lcf::win {

Window::Window() noexcept = default;

Window::~Window() noexcept = default;

Window::Window(Window &&) noexcept = default;

Window & Window::operator=(Window &&) noexcept = default;

std::error_code Window::create(const WindowCreateInfo & window_info) noexcept
{
    m_impl_up = std::make_unique<Impl>();
    return m_impl_up->create(window_info);
}

std::error_code Window::show() const noexcept
{
    return m_impl_up->show();
}

WindowHandle Window::handle() const noexcept
{
    return m_impl_up->handle();
}

std::pair<uint32_t, uint32_t> Window::getPixelSize() const noexcept
{
    return m_impl_up->getPixelSize();
}

std::span<const WindowEvent> Window::pollEvents() const noexcept
{
    return m_impl_up->pollEvents();
}

void Window::setResizeCallback(std::function<void(const ResizeEvent &)> callback) noexcept
{
    m_impl_up->setResizeCallback(std::move(callback));
}

} // namespace lcf::win
