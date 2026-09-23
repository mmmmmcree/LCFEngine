#include "system_scheduler/SystemScheduler.h"

namespace lcf::shader_toy {

void SystemScheduler::drainAndRoute()
{
    for (auto & [_, registration] : m_registrations) {
        registration.poll_events([&](details::EventPacket & packet) {
            const auto route_it = registration.routes.find(packet.m_id);
            if (route_it == registration.routes.end()) { return; }
            const auto target_it = m_registrations.find(route_it->second.target_system_p);
            if (target_it == m_registrations.end()) { return; }
            details::EventPacket forwarded = std::move(packet);
            forwarded.m_id = route_it->second.dest_id;
            target_it->second.queue_event(std::move(forwarded));
        });
    }
    for (auto & [_, registration] : m_registrations) {
        registration.publish_events();
    }
}

std::error_code SystemScheduler::tick() noexcept
{
    try {
        this->drainAndRoute();
    } catch (const std::system_error & e) {
        return e.code();
    } catch (...) {
        return std::make_error_code(std::errc::io_error);
    }
    return {};
}

} // namespace lcf::shader_toy
