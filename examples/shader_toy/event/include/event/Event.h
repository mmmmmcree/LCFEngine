#pragma once

#include "event/EventId.h"
#include "event/concepts.h"
#include "event/enums.h"
#include <cstdint>
#include <type_traits>

namespace lcf::shader_toy {

template <event_payload_c Payload, std::uint16_t system_id, EventState state>
struct Event : Payload
{
    using payload_type = Payload;
    static constexpr std::uint16_t system_id_v = system_id;
    static constexpr EventState state_v = state;
    static constexpr EventId id_v = enum_traits<EventId>::make_event_id(Payload::id_v, system_id, state);
};

template <event_payload_c Payload, std::uint16_t system_id, EventState state>
inline constexpr bool is_event_v<Event<Payload, system_id, state>> = true;

template <typename E>
concept event_c = is_event_v<std::remove_cvref_t<E>>;

template <event_c L, event_c R>
inline constexpr bool is_compatible_v = std::same_as<typename L::payload_type, typename R::payload_type>;


} // namespace lcf::shader_toy
