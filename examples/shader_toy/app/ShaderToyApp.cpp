#include "app/ShaderToyApp.h"

#include "system_scheduler/SystemScheduler.h"
#include "win/Window.h"

#include <atomic>
#include <new>
#include <thread>
#include <variant>

namespace lcf::shader_toy {

struct App::Impl
{
    std::error_code create() noexcept;
    std::error_code run() noexcept;

    win::Window m_window;
    SystemScheduler m_scheduler;
};

std::error_code App::Impl::create() noexcept
{
    win::WindowCreateInfo window_info;
    window_info.setTitle("shader toy");
    if (auto ec = m_window.create(window_info)) { return ec; }
    return m_window.show();
}

std::error_code App::Impl::run() noexcept
{
    std::atomic_bool running = true;
    std::error_code scheduler_ec;
    std::jthread scheduler_thread {[&](std::stop_token) {
        while (not scheduler_ec and running.load(std::memory_order_acquire)) {
            scheduler_ec = m_scheduler.tick();
        }
    }};
    while (not scheduler_ec and running.load(std::memory_order_acquire)) {
        for (const win::WindowEvent & event : m_window.pollEvents()) {
            if (std::holds_alternative<win::CloseEvent>(event)) {
                running.store(false, std::memory_order_release);
                break;
            }
        }
    }
    running.store(false, std::memory_order_release);
    return scheduler_ec;
}

App::App() noexcept = default;

App::~App() noexcept = default;

std::error_code App::create() noexcept
{
    if (m_impl_up) { return std::make_error_code(std::errc::operation_canceled); }
    try {
        m_impl_up = std::make_unique<Impl>();
    } catch (const std::system_error & e) {
        return e.code();
    }
    return m_impl_up->create();
}

std::error_code App::run() noexcept
{
    if (not m_impl_up) { return std::make_error_code(std::errc::operation_canceled); }
    return m_impl_up->run();
}

} // namespace lcf::shader_toy
