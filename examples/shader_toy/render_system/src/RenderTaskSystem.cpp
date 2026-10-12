#include "private_include/RenderTaskSystem.h"
#include "log.h"

namespace {

constexpr uint32_t k_cpu_worker_count = 2u;

}

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
    m_render_tasks.restart();
    m_gfx_tasks.restart();
    m_cpu_tasks.restart();
    m_cpu_workers.reserve(k_cpu_worker_count);
    for (uint32_t index = 0u; index < k_cpu_worker_count; ++index) {
        m_cpu_workers.emplace_back([this](std::stop_token token) {
            std::stop_callback stop_callback(token, [this] { m_cpu_tasks.stop(); });
            m_cpu_tasks.run();
        });
    }
    m_gfx_worker = std::jthread([this](std::stop_token token) {
        std::stop_callback stop_callback(token, [this] { m_gfx_tasks.stop(); });
        m_gfx_tasks.run();
    });
    return {};
}

std::error_code RenderTaskSystem::processTasks(bool has_frame) noexcept
{
    if (has_frame) { m_render_tasks.poll_one(); }
    else { m_render_tasks.run_one_for(std::chrono::milliseconds {10}); }
    return m_error;
}

void RenderTaskSystem::stop() noexcept
{
    m_render_tasks.stop();
    m_gfx_tasks.stop();
    m_cpu_tasks.stop();
    m_gfx_worker.request_stop();
    for (auto & worker : m_cpu_workers) { worker.request_stop(); }
}

void RenderTaskSystem::join() noexcept
{
    if (m_gfx_worker.joinable()) { m_gfx_worker.join(); }
    for (auto & worker : m_cpu_workers) {
        if (worker.joinable()) { worker.join(); }
    }
    m_cpu_workers.clear();
}

namespace task_details {

std::error_code exception_error(std::exception_ptr exception) noexcept
{
    try { std::rethrow_exception(exception); }
    catch (const std::system_error & error) { return error.code(); }
    catch (const std::exception & error) { lcf_log_error("render task exception: {}", error.what()); }
    catch (...) { lcf_log_error("render task exception"); }
    return std::make_error_code(std::errc::io_error);
}

}

}
