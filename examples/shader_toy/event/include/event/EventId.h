#pragma once

#include "enums/enum_traits.h"
#include "event/enums.h"

#include <cstdint>
#include <functional>
#include <type_traits>

namespace lcf::shader_toy {

enum class EventId : std::uint32_t {};

template <>
struct enum_traits<EventId>
{
private:
    static constexpr std::uint32_t k_payload_bits = 8;
    static constexpr std::uint32_t k_state_bits = 8;
    static constexpr std::uint32_t k_system_bits = 16;
    static constexpr std::uint32_t k_payload_mask = (std::uint32_t{1} << k_payload_bits) - 1;
    static constexpr std::uint32_t k_state_mask = (std::uint32_t{1} << k_state_bits) - 1;
    static constexpr std::uint32_t k_system_mask = (std::uint32_t{1} << k_system_bits) - 1;
    static constexpr std::uint32_t k_state_shift = k_payload_bits;
    static constexpr std::uint32_t k_system_shift = k_payload_bits + k_state_bits;
public:
    static constexpr EventId make_event_id(std::uint16_t payload_id, std::uint16_t system_id, EventState state) noexcept
    {
        return EventId{
            (static_cast<std::uint32_t>(system_id) << k_system_shift) |
            (static_cast<std::uint32_t>(state) << k_state_shift) |
            (static_cast<std::uint32_t>(payload_id) & k_payload_mask)
        };
    }
    static constexpr std::uint16_t get_payload_id(EventId id) noexcept
    {
        return static_cast<std::uint16_t>(static_cast<std::uint32_t>(id) & k_payload_mask);
    }
    static constexpr std::uint16_t get_system_id(EventId id) noexcept
    {
        return static_cast<std::uint16_t>((static_cast<std::uint32_t>(id) >> k_system_shift) & k_system_mask);
    }
    static constexpr EventState get_state(EventId id) noexcept
    {
        return static_cast<EventState>((static_cast<std::uint32_t>(id) >> k_state_shift) & k_state_mask);
    }
};

} // namespace lcf::shader_toy

template <>
struct std::hash<lcf::shader_toy::EventId>
{
    using underlying_type = std::underlying_type_t<lcf::shader_toy::EventId>;
    std::size_t operator()(lcf::shader_toy::EventId id) const noexcept
    {
        return std::hash<underlying_type>{}(static_cast<underlying_type>(id));
    }
};
