#include "private_include/RenderTaskSystem.h"
#include <asio/post.hpp>

namespace lcf::shader_toy {

RenderTaskSystem::RenderTaskSystem() noexcept = default;

RenderTaskSystem::~RenderTaskSystem() noexcept
{
    this->stop();
    this->join();
}

std::error_code RenderTaskSystem::run() noexcept
{
    if (m_gfx_worker.joinable()) { return std::make_error_code(std::errc::operation_in_progress); }
    if (m_error) { return m_error; }
    m_render_io_context.restart();
    m_gfx_io_context.restart();
    m_gfx_worker = std::jthread([this](std::stop_token token) {
        std::stop_callback stop_callback(token, [this] { m_gfx_io_context.stop(); });
        m_gfx_io_context.run();
    });
    return {};
}

std::error_code RenderTaskSystem::processOne() noexcept
{
    m_render_io_context.poll_one();
    return m_error;
}

std::error_code RenderTaskSystem::waitOne() noexcept
{
    m_render_io_context.run_one();
    return m_error;
}

void RenderTaskSystem::reportError(std::error_code error)
{
    asio::post(m_render_io_context, [this, error] { m_error = error; });
}

void RenderTaskSystem::stop() noexcept
{
    m_render_io_context.stop();
    m_gfx_io_context.stop();
    m_gfx_worker.request_stop();
}

void RenderTaskSystem::join() noexcept
{
    if (m_gfx_worker.joinable()) { m_gfx_worker.join(); }
}

}
