#include "task_system/TaskSystem.h"

#include <utility>

namespace {

using namespace lcf::shader_toy;

void dispatch_packet(
    const details::EventPacket & packet,
    RobinMap<EventId, EventHandler> & handlers,
    RobinMap<std::error_code, EventErrorHandler, ErrorCodeHash> & error_handlers
);

void drain_in_mailbox(
    EventQueue & in_mailbox,
    RobinMap<EventId, EventHandler> & handlers,
    RobinMap<std::error_code, EventErrorHandler, ErrorCodeHash> & error_handlers
);

}

namespace lcf::shader_toy {

TaskSystem::~TaskSystem() noexcept
{
    this->stop();
}

void TaskSystem::publishEvents() noexcept
{
    m_in_mailbox.publish();
    asio::post(m_io_context, [this] {
        drain_in_mailbox(m_in_mailbox, m_handlers, m_error_handlers);
        m_out_mailbox.publish();
    });
}

std::error_code TaskSystem::run() noexcept
{
    if (m_worker.joinable()) { return std::make_error_code(std::errc::operation_in_progress); }
    m_io_context.restart();
    m_work_guard_opt.emplace(m_io_context.get_executor());
    m_worker = std::jthread([this](std::stop_token token) {
        std::stop_callback stop_callback(token, [this] {
            m_work_guard_opt.reset();
            m_io_context.stop();
        });
        m_io_context.run();
    });
    return {};
}

void TaskSystem::stop() noexcept
{
    m_work_guard_opt.reset();
    m_io_context.stop();
    m_worker.request_stop();
    if (m_worker.joinable()) {
        m_worker.join();
    }
}

} // namespace lcf::shader_toy

namespace {

void dispatch_packet(
    const details::EventPacket & packet,
    RobinMap<EventId, EventHandler> & handlers,
    RobinMap<std::error_code, EventErrorHandler, ErrorCodeHash> & error_handlers
)
{
    const auto handler_it = handlers.find(packet.m_id);
    if (handler_it == handlers.end()) { return; }
    const std::error_code ec = handler_it.value()(packet);
    if (not ec) { return; }
    const auto error_it = error_handlers.find(ec);
    if (error_it == error_handlers.end()) { return; }
    error_it.value()(packet);
}

void drain_in_mailbox(
    EventQueue & in_mailbox,
    RobinMap<EventId, EventHandler> & handlers,
    RobinMap<std::error_code, EventErrorHandler, ErrorCodeHash> & error_handlers
)
{
    for (details::EventPacket & packet : in_mailbox.pollEvents()) {
        dispatch_packet(packet, handlers, error_handlers);
    }
}

}
