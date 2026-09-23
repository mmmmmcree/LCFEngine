#pragma once

#include "event/EventHandler.h"
#include "event/EventQueue.h"
#include "containers/RobinMap.h"

#include <asio/executor_work_guard.hpp>
#include <asio/io_context.hpp>
#include <asio/post.hpp>

#include <concepts>
#include <functional>
#include <optional>
#include <system_error>
#include <thread>
#include <type_traits>
#include <utility>

namespace lcf::shader_toy {

class TaskSystem;

namespace details {

template <event_c E, EventState State>
struct make_task_event_handler
{
    template <typename F>
    requires std::is_nothrow_invocable_r_v<std::error_code, F &, const E &>
    static EventHandler create(TaskSystem *, F && handler)
    {
        return make_event_handler<E>(std::forward<F>(handler));
    }
};

} // namespace details

class TaskSystem
{
    using Self = TaskSystem;
    using WorkGuard = asio::executor_work_guard<asio::io_context::executor_type>;

    template <event_c E, EventState State>
    friend struct details::make_task_event_handler;
public:
    ~TaskSystem() noexcept;
    TaskSystem() noexcept = default;
    TaskSystem(const Self &) = delete;
    TaskSystem(Self &&) = delete;
    Self & operator=(const Self &) = delete;
    Self & operator=(Self &&) = delete;
public:
    auto pollEvents() noexcept { return m_out_mailbox.pollEvents(); }
    void queueEvent(details::EventPacket packet) noexcept { m_in_mailbox.push(std::move(packet)); }
    void publishEvents() noexcept;
    template <event_c E, typename F>
    requires std::is_nothrow_invocable_r_v<std::error_code, F &, const E &>
    void registerHandler(F && handler)
    {
        m_handlers.insert_or_assign(E::id_v, details::make_task_event_handler<E, E::state_v>::create(this, std::forward<F>(handler)));
    }
    template <typename F>
    requires std::is_nothrow_invocable_v<F &, const details::EventPacket &>
    void registerErrorHandler(std::error_code ec, F && handler)
    {
        m_error_handlers.insert_or_assign(ec, std::forward<F>(handler));
    }
    std::error_code run() noexcept;
    void stop() noexcept;
private:
    template <typename E>
    requires event_c<std::remove_cvref_t<E>>
    void emit(E && event)
    {
        m_out_mailbox.push(std::forward<E>(event));
    }
public:
    asio::io_context m_io_context;
    std::optional<WorkGuard> m_work_guard_opt;
    EventQueue m_in_mailbox;
    EventQueue m_out_mailbox;
    RobinMap<EventId, EventHandler> m_handlers;
    RobinMap<std::error_code, EventErrorHandler, ErrorCodeHash> m_error_handlers;
    std::jthread m_worker;
};

namespace details {

template <event_c E>
struct make_task_event_handler<E, EventState::eRequest>
{
    template <typename F>
    requires std::is_nothrow_invocable_r_v<std::error_code, F &, const E &>
    static EventHandler create(TaskSystem * system_p, F && handler)
    {
        return [system_p, handler = std::forward<F>(handler)](const details::EventPacket & packet) mutable noexcept -> std::error_code {
            E event = details::event_as<E>(packet);
            if (const std::error_code ec = std::invoke(handler, event)) { return ec; }
            using Completion = Event<typename E::payload_type, E::system_id_v, EventState::eCompletion>;
            system_p->emit(Completion{static_cast<const typename E::payload_type &>(event)});
            return {};
        };
    }
};

} // namespace details

} // namespace lcf::shader_toy
