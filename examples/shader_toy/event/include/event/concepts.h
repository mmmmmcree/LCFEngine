#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace lcf::shader_toy {

inline constexpr std::size_t k_max_event_size = 128;

template <typename T>
inline constexpr bool is_event_v = false;

template <typename P>
concept event_payload_c = std::is_object_v<P> and
    (sizeof(P) <= k_max_event_size) and
    (alignof(P) <= alignof(std::max_align_t)) and
    std::is_nothrow_move_constructible_v<P> and
    std::is_nothrow_destructible_v<P> and
    requires {
        { P::id_v } -> std::convertible_to<std::uint16_t>;
    };

} // namespace lcf::shader_toy
