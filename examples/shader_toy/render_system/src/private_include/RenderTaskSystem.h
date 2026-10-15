#pragma once

#include <thread>
#include <asio/any_io_executor.hpp>
#include <asio/executor_work_guard.hpp>
#include <asio/io_context.hpp>
#include <system_error>
#include <utility>
#include <vector>

namespace lcf::shader_toy {

class RenderTaskSystem
{
    using Self = RenderTaskSystem;
    using WorkGuard = asio::executor_work_guard<asio::io_context::executor_type>;
    using WorkerList = std::vector<std::jthread>;
public:
    ~RenderTaskSystem() noexcept;
    RenderTaskSystem() noexcept;
    RenderTaskSystem(const Self &) = delete;
    Self & operator=(const Self &) = delete;
    RenderTaskSystem(Self &&) = delete;
    Self & operator=(Self &&) = delete;
public:
    template <typename Task>
    void enqueue(Task && task)
    {
        std::move(task).start(m_spawn_thread_io_context.get_executor(), [this](const auto & result) {
            if (not result) { m_error = result.error(); }
        });
    }
    asio::any_io_executor getSpawnThreadExecutor() noexcept { return m_spawn_thread_io_context.get_executor(); }
    asio::any_io_executor getGfxExecutor() noexcept { return m_gfx_io_context.get_executor(); }
    asio::any_io_executor getCpuExecutor() noexcept { return m_cpu_io_context.get_executor(); }
    std::error_code run() noexcept;
    std::error_code processOne() noexcept;
    void stop() noexcept;
    void join() noexcept;
private:
    asio::io_context m_spawn_thread_io_context;
    asio::io_context m_gfx_io_context;
    asio::io_context m_cpu_io_context;
    WorkGuard m_render_work_guard = asio::make_work_guard(m_spawn_thread_io_context);
    WorkGuard m_gfx_work_guard = asio::make_work_guard(m_gfx_io_context);
    WorkGuard m_cpu_work_guard = asio::make_work_guard(m_cpu_io_context);
    std::error_code m_error;
    std::jthread m_gfx_worker;
    WorkerList m_cpu_workers;
};

}
