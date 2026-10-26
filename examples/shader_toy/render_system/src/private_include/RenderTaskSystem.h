#pragma once

#include <thread>
#include <asio/any_io_executor.hpp>
#include <asio/executor_work_guard.hpp>
#include <asio/io_context.hpp>
#include <system_error>

namespace lcf::shader_toy {

class RenderTaskSystem
{
    using Self = RenderTaskSystem;
    using WorkGuard = asio::executor_work_guard<asio::io_context::executor_type>;
public:
    ~RenderTaskSystem() noexcept;
    RenderTaskSystem() noexcept;
    RenderTaskSystem(const Self &) = delete;
    Self & operator=(const Self &) = delete;
    RenderTaskSystem(Self &&) = delete;
    Self & operator=(Self &&) = delete;
public:
    asio::any_io_executor getRenderExecutor() noexcept { return m_render_io_context.get_executor(); }
    asio::any_io_executor getGfxExecutor() noexcept { return m_gfx_io_context.get_executor(); }
    void reportError(std::error_code error);
    std::error_code run() noexcept;
    std::error_code processOne() noexcept;
    std::error_code waitOne() noexcept;
    void stop() noexcept;
    void join() noexcept;
private:
    asio::io_context m_render_io_context;
    asio::io_context m_gfx_io_context;
    WorkGuard m_render_work_guard = asio::make_work_guard(m_render_io_context);
    WorkGuard m_gfx_work_guard = asio::make_work_guard(m_gfx_io_context);
    std::error_code m_error;
    std::jthread m_gfx_worker;
};

}
