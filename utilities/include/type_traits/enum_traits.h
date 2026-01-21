#pragma once

#include <type_traits>

namespace lcf {
    template <typename Enum>
    struct is_enum_flags : std::false_type {};

    template <typename Enum>
    inline constexpr bool is_enum_flags_v = is_enum_flags<Enum>::value;

    template <typename... Enums>
    struct is_enum_family : std::false_type {};

    template <typename... Enums>
    inline constexpr bool is_enum_family_v = is_enum_family<Enums...>::value;
}