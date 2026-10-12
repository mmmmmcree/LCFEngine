#pragma once

#include "AsyncTaskChain.h"
#include <asio/executor_work_guard.hpp>
#include <asio/io_context.hpp>
#include <thread>
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
    template <typename T>
    void enqueue(const AsyncTaskChain<T> & chain)
    {
        chain.start(m_render_tasks.get_executor(), [this](AsyncTaskResult<T> result) {
            if (not result.getResult()) { m_error = result.getResult().error(); }
        });
    }
    asio::any_io_executor getRenderExecutor() noexcept { return m_render_tasks.get_executor(); }
    asio::any_io_executor getGfxExecutor() noexcept { return m_gfx_tasks.get_executor(); }
    asio::any_io_executor getCpuExecutor() noexcept { return m_cpu_tasks.get_executor(); }
    std::error_code run() noexcept;
    std::error_code processTasks(bool has_frame) noexcept;
    void stop() noexcept;
    void join() noexcept;
private:
    asio::io_context m_render_tasks;
    asio::io_context m_gfx_tasks;
    asio::io_context m_cpu_tasks;
    WorkGuard m_render_work_guard = asio::make_work_guard(m_render_tasks);
    WorkGuard m_gfx_work_guard = asio::make_work_guard(m_gfx_tasks);
    WorkGuard m_cpu_work_guard = asio::make_work_guard(m_cpu_tasks);
    std::error_code m_error;
    std::jthread m_gfx_worker;
    WorkerList m_cpu_workers;
};

}
