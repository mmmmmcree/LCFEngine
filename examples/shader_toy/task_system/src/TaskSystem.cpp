#include "task_system/TaskSystem.h"
#include "task_system/task_system_events.h"
#include "FileWatcher.h"
#include <asio/post.hpp>

#include <type_traits>
#include <utility>
#include <variant>

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

void dispatch_file_action(
    FileAction & action,
    RobinMap<EventId, EventHandler> & handlers,
    RobinMap<std::error_code, EventErrorHandler, ErrorCodeHash> & error_handlers,
    EventQueue & out_mailbox
);

}

namespace lcf::shader_toy {

TaskSystem::TaskSystem() noexcept = default;

TaskSystem::~TaskSystem() noexcept
{
    this->stop();
}

std::error_code TaskSystem::registerService(TaskSystemService service) noexcept
{
    switch (service) {
        case TaskSystemService::eFileWatcher: {
            if (m_file_watcher_up) { return std::make_error_code(std::errc::operation_in_progress); }
            m_file_watcher_up = std::make_unique<FileWatcher>([this](FileAction action) {
                asio::post(m_io_context, [this, action = std::move(action)]() mutable {
                    dispatch_file_action(action, m_handlers, m_error_handlers, m_out_mailbox);
                });
            });
            this->registerHandler<WatchDirectoryEvent>([this](const WatchDirectoryEvent & event) noexcept {
                return m_file_watcher_up->addWatchedDirectory(event.path, event.recursive);
            });
        } break;
    }
    return {};

}

void TaskSystem::publishEvents() noexcept
{
    m_in_mailbox.publish();
    if (m_publish_scheduled.exchange(true, std::memory_order_acq_rel)) { return; }
    asio::post(m_io_context, [this] {
        drain_in_mailbox(m_in_mailbox, m_handlers, m_error_handlers);
        m_out_mailbox.publish();
        m_publish_scheduled.store(false, std::memory_order_release);
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
    if (m_worker.joinable()) { m_worker.join(); }
    m_publish_scheduled.store(false, std::memory_order_release);
    m_file_watcher_up.reset();
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

void dispatch_file_action(
    FileAction & action,
    RobinMap<EventId, EventHandler> & handlers,
    RobinMap<std::error_code, EventErrorHandler, ErrorCodeHash> & error_handlers,
    EventQueue & out_mailbox
)
{
    std::visit([&](auto & file_action) {
        using T = std::decay_t<decltype(file_action)>;
        if constexpr (std::is_same_v<T, FileModify>) {
            FileModifiedEvent event;
            event.path = std::move(file_action.path);
            details::EventPacket packet = details::make_event_packet(std::move(event));
            dispatch_packet(packet, handlers, error_handlers);
        } else if constexpr (std::is_same_v<T, FileAdd>) {
            FileAddedEvent event;
            event.path = std::move(file_action.path);
            out_mailbox.push(std::move(event));
        } else if constexpr (std::is_same_v<T, FileDelete>) {
            FileDeletedEvent event;
            event.path = std::move(file_action.path);
            out_mailbox.push(std::move(event));
        } else if constexpr (std::is_same_v<T, FileMove>) {
            FileMovedEvent event;
            event.path = std::move(file_action.path);
            event.old_path = std::move(file_action.old_path);
            out_mailbox.push(std::move(event));
        }
    }, action);
}

}
