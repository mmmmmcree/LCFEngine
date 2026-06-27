#include "win/Window.h"
#include "win/error.h"
#include <atomic>
#include <vector>
#include <span>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

namespace {

using namespace lcf::win;

static std::atomic<int> s_init_refcount {0};

bool initialize() noexcept;

void quit() noexcept;

void apply_glfw_window_hints(lcf::win::WindowFlags window_flags) noexcept;

} // anonymous namespace

namespace lcf::win {

struct Window::Impl
{
    static Impl * from(GLFWwindow * window) noexcept;
    static void framebuffer_size_callback(GLFWwindow * window, int width, int height);
    static void window_close_callback(GLFWwindow * window);

    ~Impl();

    std::error_code create(const WindowCreateInfo & window_info) noexcept;
    std::error_code show() const noexcept;
    WindowHandle handle() const noexcept;
    std::pair<uint32_t, uint32_t> getPixelSize() const noexcept;
    std::span<const WindowEvent> pollEvents() noexcept;
    void setResizeCallback(std::function<void(const ResizeEvent &)> callback) noexcept;

    GLFWwindow * m_window = nullptr;
    bool m_is_initialized = false;
    std::vector<WindowEvent> m_events;
    std::function<void(const ResizeEvent &)> m_resize_callback = [](const ResizeEvent &) {};
};

Window::Impl * Window::Impl::from(GLFWwindow * window) noexcept
{
    return static_cast<Impl *>(glfwGetWindowUserPointer(window));
}

//- resize fires synchronously here, even during a Win32 modal resize drag — that
//- is why it uses a callback rather than the pulled event stream.
void Window::Impl::framebuffer_size_callback(GLFWwindow * window, int width, int height)
{
    auto * self = from(window);
    ResizeEvent resize_event { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
    self->m_resize_callback(resize_event);
}

void Window::Impl::window_close_callback(GLFWwindow * window)
{
    auto * self = from(window);
    self->m_events.emplace_back(CloseEvent {});
}

Window::Impl::~Impl()
{
    if (m_window) {
        glfwDestroyWindow(m_window);
    }
    if (m_is_initialized) {
        quit();
    }
}

std::error_code Window::Impl::create(const WindowCreateInfo & window_info) noexcept
{
    if (not initialize()) { return make_error_code(errc::backend_init_failed); }
    m_is_initialized = true;
    GLFWmonitor * primary = glfwGetPrimaryMonitor();
    const GLFWvidmode * mode = glfwGetVideoMode(primary);
    if (not mode) { return make_error_code(errc::display_query_failed); }
    const int32_t width = static_cast<int32_t>(static_cast<float>(mode->width) * window_info.getWidthRatio());
    const int32_t height = static_cast<int32_t>(static_cast<float>(mode->height) * window_info.getHeightRatio());

    apply_glfw_window_hints(window_info.getFlags());
    GLFWmonitor * target_monitor = contains_flags(window_info.getFlags(), WindowFlags::eFullscreen) ? primary : nullptr;
    m_window = glfwCreateWindow(
        width, height,
        window_info.getTitle().c_str(), target_monitor, nullptr);
    if (not m_window) { return make_error_code(errc::window_creation_failed); }
    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, &Impl::framebuffer_size_callback);
    glfwSetWindowCloseCallback(m_window, &Impl::window_close_callback);
    return {};
}

std::error_code Window::Impl::show() const noexcept
{
    glfwShowWindow(m_window);
    return {};
}

WindowHandle Window::Impl::handle() const noexcept
{
#if defined(_WIN32)
    return win32::WindowHandle(
        static_cast<void *>(GetModuleHandleW(nullptr)),
        static_cast<void *>(glfwGetWin32Window(m_window)));
#elif defined(__APPLE__)
    return metal::WindowHandle(glfwGetCocoaWindow(m_window));
#else
    if (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND) {
        return wayland::WindowHandle(
            static_cast<void *>(glfwGetWaylandDisplay()),
            static_cast<void *>(glfwGetWaylandWindow(m_window)));
    }
    return xcb::WindowHandle(
        static_cast<void *>(glfwGetX11Display()),
        static_cast<uint32_t>(glfwGetX11Window(m_window)));
#endif
}

std::pair<uint32_t, uint32_t> Window::Impl::getPixelSize() const noexcept
{
    int w = 0, h = 0;
    glfwGetFramebufferSize(m_window, &w, &h);
    return { static_cast<uint32_t>(w), static_cast<uint32_t>(h) };
}

std::span<const WindowEvent> Window::Impl::pollEvents() noexcept
{
    m_events.clear();
    glfwPollEvents();   //- GLFW callbacks append to m_events during this call
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
    if (not glfwInit()) {
        s_init_refcount.fetch_sub(1, std::memory_order_acq_rel);
        return false;
    }
    return true;
}

void quit() noexcept
{
    if (s_init_refcount.fetch_sub(1, std::memory_order_acq_rel) != 1) { return; }
    glfwTerminate();
}

void apply_glfw_window_hints(lcf::win::WindowFlags window_flags) noexcept
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);   //- window only vends a native handle; no GL context
    glfwWindowHint(GLFW_RESIZABLE, lcf::contains_flags(window_flags, WindowFlags::eResizable) ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_DECORATED, lcf::contains_flags(window_flags, WindowFlags::eBorderless) ? GLFW_FALSE : GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE, lcf::contains_flags(window_flags, WindowFlags::eHidden) ? GLFW_FALSE : GLFW_TRUE);
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
