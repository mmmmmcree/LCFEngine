#include "system_scheduler/SystemScheduler.h"

namespace lcf::shader_toy {

void SystemScheduler::tick() noexcept
{
    Registration * current_registration = nullptr;
    EventVisitor visitor = [&](details::EventPacket & packet) noexcept {
        const auto & registration = *current_registration;
        const auto route_it = registration.m_routes.find(packet.m_id);
        if (route_it == registration.m_routes.end()) { return; }
        const auto & [system_p, dest_event_id] = route_it->second;
        const auto target_it = m_registrations.find(system_p);
        if (target_it == m_registrations.end()) { return; }
        packet.m_id = dest_event_id;
        target_it.value().m_queue_event(std::move(packet));
    };
    for (auto it = m_registrations.begin(); it != m_registrations.end(); ++it) {
        current_registration = &it.value();
        current_registration->m_poll_events(visitor);
    }
    for (auto it = m_registrations.begin(); it != m_registrations.end(); ++it) {
        it.value().m_publish_events();
    }
}

} // namespace lcf::shader_toy
