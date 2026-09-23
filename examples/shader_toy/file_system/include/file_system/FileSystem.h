#pragma once

#include "event/EventHandler.h"
#include "event/EventQueue.h"
#include "containers/RobinMap.h"

#include <concepts>
#include <filesystem>
#include <functional>
#include <memory>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace lcf::shader_toy {

class FileWatcher;

class FileSystem
{
    using Self = FileSystem;
public:
    ~FileSystem() noexcept;
    FileSystem() noexcept;
    FileSystem(const Self &) = delete;
    FileSystem(Self &&) = delete;
    Self & operator=(const Self &) = delete;
    Self & operator=(Self &&) = delete;
public:
    std::error_code addWatchedDirectory( const std::filesystem::path & directory) noexcept;
    std::error_code watch() noexcept;
    auto pollEvents() noexcept { return m_out_mailbox.pollEvents(); }
    void queueEvent(details::EventPacket packet) noexcept { m_in_mailbox.push(std::move(packet)); }
    void publishEvents() noexcept { m_in_mailbox.publish(); }
    template <event_c E, typename F>
    requires std::is_nothrow_invocable_r_v<std::error_code, F &, const E &>
    void registerHandler(F && handler)
    {
        m_handlers.insert_or_assign(E::id_v, make_event_handler<E>(std::forward<F>(handler)));
    }
    template <typename F>
    requires std::is_nothrow_invocable_v<F &, const details::EventPacket &>
    void registerErrorHandler(std::error_code ec, F && handler)
    {
        m_error_handlers.insert_or_assign(ec, std::forward<F>(handler));
    }
    std::error_code run() noexcept;
    void stop() noexcept;
public:
    std::unique_ptr<FileWatcher> m_file_watcher_up;
    EventQueue m_in_mailbox;
    EventQueue m_out_mailbox;
    RobinMap<EventId, EventHandler> m_handlers;
    RobinMap<std::error_code, EventErrorHandler, ErrorCodeHash> m_error_handlers;
    std::jthread m_worker;
};

} // namespace lcf::shader_toy
