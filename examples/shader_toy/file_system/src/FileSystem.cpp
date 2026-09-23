#include "file_system/FileSystem.h"
#include "FileWatcher.h"
#include "file_system/file_system_events.h"

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

void enqueue_file_action(FileAction & action, EventQueue & out_mailbox);

}

namespace lcf::shader_toy {

FileSystem::~FileSystem() noexcept
{
    this->stop();
}

FileSystem::FileSystem() noexcept
{
    m_file_watcher_up = std::make_unique<FileWatcher>();
}

std::error_code FileSystem::addWatchedDirectory(const std::filesystem::path & directory) noexcept
{
    return m_file_watcher_up->addWatchedDirectory(directory);
}

std::error_code FileSystem::watch() noexcept
{
    return m_file_watcher_up->watch();
}

std::error_code FileSystem::run() noexcept
{
    if (m_worker.joinable()) {
        return std::make_error_code(std::errc::operation_in_progress);
    }
    m_worker = std::jthread([this](std::stop_token token) {
        while (not token.stop_requested()) {
            for (FileAction & action : m_file_watcher_up->pollEvents()) {
                enqueue_file_action(action, m_out_mailbox);
            }
            for (details::EventPacket & packet : m_in_mailbox.pollEvents()) {
                dispatch_packet(packet, m_handlers, m_error_handlers);
            }
            m_out_mailbox.publish();
        }
    });
    return {};
}

void FileSystem::stop() noexcept
{
    m_worker.request_stop();
    if (m_worker.joinable()) { m_worker.join(); }
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

void enqueue_file_action(FileAction & action, EventQueue & out_mailbox)
{
    std::visit([&](auto & file_action) {
        using T = std::decay_t<decltype(file_action)>;
        if constexpr (std::is_same_v<T, FileModify>) {
            Event<FileModifiedPayload, k_file_system_id, EventState::eRequest> event;
            event.path = std::move(file_action.m_path);
            out_mailbox.push(std::move(event));
        } else if constexpr (std::is_same_v<T, FileAdd>) {
            Event<FileAddedPayload, k_file_system_id, EventState::eRequest> event;
            event.path = std::move(file_action.m_path);
            out_mailbox.push(std::move(event));
        } else if constexpr (std::is_same_v<T, FileDelete>) {
            Event<FileDeletedPayload, k_file_system_id, EventState::eRequest> event;
            event.path = std::move(file_action.m_path);
            out_mailbox.push(std::move(event));
        } else if constexpr (std::is_same_v<T, FileMove>) {
            Event<FileMovedPayload, k_file_system_id, EventState::eRequest> event;
            event.path = std::move(file_action.m_path);
            event.old_path = std::move(file_action.m_old_path);
            out_mailbox.push(std::move(event));
        }
    }, action);
}

}
