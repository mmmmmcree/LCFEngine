#pragma once

#include "event/EventQueue.h"

#include <concepts>
#include <system_error>
#include <utility>

namespace lcf::shader_toy {

template <typename System>
concept system_c = requires(System & system, details::EventPacket queued) {
    system.pollEvents();
    system.queueEvent(std::move(queued));
    system.publishEvents();
    { system.run() } -> std::same_as<std::error_code>;
};

} // namespace lcf::shader_toy
