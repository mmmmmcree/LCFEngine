#include "app/ShaderToyApp.h"
#include "system_scheduler/SystemScheduler.h"
#include "task_system/TaskSystem.h"
#include "task_system/task_system_events.h"
#include "win/Window.h"
#include "log.h"
#include "VirtualPathRegistry.h"
#include <atomic>
#include <utility>
#include <variant>

namespace lcf::shader_toy {

struct App::Impl
{
    std::error_code create() noexcept;
    std::error_code run() noexcept;

    win::Window m_window;
    SystemScheduler m_scheduler;
    TaskSystem m_task_system;
};

std::error_code App::Impl::create() noexcept
{
    win::WindowCreateInfo window_info;
    window_info.setTitle("shader toy");
    if (auto ec = m_window.create(window_info)) { return ec; }
    if (auto ec = m_window.show()) { return ec; }
    if (auto ec = m_task_system.registerService(TaskSystemService::eFileWatcher)) { return ec; }
    m_task_system.registerHandler<FileModifiedEvent>(
        [](const FileModifiedEvent & event) noexcept -> std::error_code {
            lcf_log_info("file modified: {}", event.path.string());
            return {};
        }
    );
    m_scheduler.registerSystem(m_task_system);
    m_task_system.queueEvent(std::move(WatchDirectoryEvent {{.path = SHADER_ASSETS_DIR}}));
    return {};
}

std::error_code App::Impl::run() noexcept
{
    if (auto ec = m_task_system.run()) { return ec; }
    std::atomic_bool running = true;
    std::error_code scheduler_ec;
    while (running.load(std::memory_order_acquire)) {
        for (const win::WindowEvent & event : m_window.pollEvents()) {
            if (std::holds_alternative<win::CloseEvent>(event)) {
                running.store(false, std::memory_order_release);
                break;
            }
        }
        m_scheduler.tick();
    }
    running.store(false, std::memory_order_release);
    m_task_system.stop();
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
