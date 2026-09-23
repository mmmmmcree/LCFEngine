#pragma once

#include "event/Event.h"
#include "event/EventId.h"
#include "event/EventQueue.h"
#include "containers/RobinMap.h"
#include "functional/UniqueFunction.h"
#include <system_error>
#include <type_traits>
#include <utility>

namespace lcf::shader_toy {

template <typename System>
concept system_c = requires(System & system) {
    system.pollEvents();
    system.publishEvents();
};

class SystemScheduler
{
    using Self = SystemScheduler;
    using EventVisitor = UniqueFunction<void(details::EventPacket &) noexcept>;
    using Route = std::pair<void *, EventId>;
    struct Registration
    {
        using PollEventsFunction = UniqueFunction<void(EventVisitor &) noexcept>;
        using QueueEventFunction = UniqueFunction<void(details::EventPacket) noexcept>;
        using PublishEventsFunction = UniqueFunction<void() noexcept>;
        Registration(
            PollEventsFunction poll_events,
            QueueEventFunction queue_event,
            PublishEventsFunction publish_events) noexcept :
            m_poll_events(std::move(poll_events)),
            m_queue_event(std::move(queue_event)),
            m_publish_events(std::move(publish_events)) {}
        void addRoute(EventId id, Route route) noexcept { m_routes.insert_or_assign(id, route); }
        PollEventsFunction m_poll_events;
        QueueEventFunction m_queue_event;
        PublishEventsFunction m_publish_events;
        RobinMap<EventId, Route> m_routes;
    };
public:
    ~SystemScheduler() noexcept = default;
    SystemScheduler() noexcept = default;
    SystemScheduler(const Self &) = delete;
    SystemScheduler(Self &&) = delete;
    Self & operator=(const Self &) = delete;
    Self & operator=(Self &&) = delete;
public:
    template <system_c S>
    void registerSystem(S & system)
    {
        m_registrations.emplace(&system, Registration{
            [&system](EventVisitor & visitor) noexcept { for (auto && packet : system.pollEvents()) { visitor(packet); } },
            [&system](details::EventPacket packet) noexcept { system.queuePacket(std::move(packet)); },
            [&system] noexcept { system.publishEvents(); }
        });
    }
    template <event_c From, event_c To, system_c Source, system_c Target>
    requires is_compatible_v<From, To>
    void registerRoute(Source & source, Target & target) noexcept
    {
        const auto source_it = m_registrations.find(&source);
        if (source_it == m_registrations.end()) { return; }
        if (not m_registrations.contains(&target)) { return; }
        source_it.value().addRoute(From::id_v, std::make_pair(&target, To::id_v));
    }
    void tick() noexcept;
private:
    RobinMap<void *, Registration> m_registrations;
};

} // namespace lcf::shader_toy
