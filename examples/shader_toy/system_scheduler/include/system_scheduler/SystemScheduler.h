#pragma once

#include "event/Event.h"
#include "event/EventId.h"
#include "event/EventQueue.h"
#include "system/system_concept.h"
#include "containers/RobinMap.h"

#include <functional>
#include <system_error>
#include <type_traits>
#include <utility>

namespace lcf::shader_toy {

class SystemScheduler
{
    using Self = SystemScheduler;
    using EventVisitor = std::function<void(details::EventPacket &)>;

    struct Route
    {
        void * target_system_p = nullptr;
        EventId dest_id{};
    };

    struct Registration
    {
        std::function<void(const EventVisitor &)> poll_events;
        std::function<void(details::EventPacket)> queue_event;
        std::function<void()> publish_events;
        RobinMap<EventId, Route> routes;
    };
public:
    ~SystemScheduler() noexcept = default;
    SystemScheduler() noexcept = default;
    SystemScheduler(const Self &) = delete;
    SystemScheduler(Self &&) = delete;
    Self & operator=(const Self &) = delete;
    Self & operator=(Self &&) = delete;

    template <system_c S>
    void registerSystem(S & system)
    {
        Registration registration;
        registration.poll_events = [&system](const EventVisitor & visit) {
            for (auto && packet : system.pollEvents()) {
                visit(packet);
            }
        };
        registration.queue_event = [&system](details::EventPacket packet) {
            system.queueEvent(std::move(packet));
        };
        registration.publish_events = [&system] {
            system.publishEvents();
        };
        m_registrations.emplace(&system, std::move(registration));
    }

    template <event_c From, event_c To, system_c Source, system_c Target>
    requires std::is_layout_compatible_v<From, To>
    void registerRoute(Source & source, Target & target)
    {
        const auto source_it = m_registrations.find(&source);
        if (source_it == m_registrations.end()) { return; }
        if (not m_registrations.contains(&target)) { return; }
        source_it.value().routes.insert_or_assign(
            From::id_v,
            Route{.target_system_p = &target, .dest_id = To::id_v}
        );
    }

    std::error_code tick() noexcept;
private:
    void drainAndRoute();
private:
    RobinMap<void *, Registration> m_registrations;
};

} // namespace lcf::shader_toy
