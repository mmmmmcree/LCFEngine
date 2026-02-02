#pragma once

#include <type_traits>
#include <bit>

namespace lcf {
    //- size_of
    template <typename T>
    inline constexpr size_t size_of_v = sizeof(T);

    template <typename T>
    struct size_of : std::integral_constant<size_t, size_of_v<T>> {};

    //- alignment_of
    template <typename T>
    inline constexpr size_t alignment_of_v = std::alignment_of_v<T>;

    template <typename T>
    struct alignment_of : std::integral_constant<size_t, alignment_of_v<T>> {};

    template <typename T>
    inline constexpr bool is_floating_point_v = std::is_floating_point_v<T>;

    template <typename T>
    struct is_floating_point : std::bool_constant<is_floating_point_v<T>> {};

    template <typename T>
    inline constexpr bool is_integral_v = std::is_integral_v<T>;

    template <typename T>
    struct is_integral : std::bool_constant<is_integral_v<T>> {};

    template <typename T>
    inline constexpr bool is_arithmetic_v = is_floating_point_v<T> || is_integral_v<T>;

    template <typename T>
    struct is_arithmetic : std::bool_constant<is_arithmetic_v<T>> {};

    template <typename Enum>
    inline constexpr bool is_enum_flags_v = false;

    template <typename Enum>
    struct is_enum_flags : std::bool_constant<is_enum_flags_v<Enum>> {};

    template <typename Src, typename Dst>
    inline constexpr bool is_convertible_v = std::is_convertible_v<Src, Dst>;

    template <typename Src, typename Dst>
    struct is_convertible : std::bool_constant<is_convertible_v<Src, Dst>> {};

    template <typename Src, typename Dst>
    inline constexpr bool is_pointer_type_convertible_v = false;

    template <typename Src, typename Dst>
    struct is_pointer_type_convertible : std::bool_constant<is_pointer_type_convertible_v<Src, Dst>> {};
}   