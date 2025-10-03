#pragma once

#include <type_traits>
#include <bit>

namespace lcf {
    template <typename T>
    concept enum_c = std::is_enum_v<T>;

    template <enum_c Enum>
    struct enum_flags_true_type 
    {
        using enum_type = std::remove_cvref_t<Enum>;
        static constexpr bool is_flags = true;
    };

    template <enum_c Enum>
    struct enum_flags_false_type
    {
        using enum_type = std::remove_cvref_t<Enum>;
        static constexpr bool is_flags = false;
    };

    template <enum_c Enum>
    struct enum_flags_traits : enum_flags_false_type<Enum> { };

    template <enum_c Enum>
    static constexpr bool enum_flags_traits_v = enum_flags_traits<std::remove_cvref_t<Enum>>::is_flags;

    template <enum_c Enum>
    using enum_flags_traits_t = typename enum_flags_traits<std::remove_cvref_t<Enum>>::enum_type;

    template <typename T>
    concept enum_flags_c = enum_c<T> and enum_flags_traits_v<T>;
}