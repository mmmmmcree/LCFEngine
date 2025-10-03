#pragma once

#include <type_traits>
#include <bit>

namespace lcf {
    //- size_of
    template <typename T>
    struct size_of : std::integral_constant<size_t, sizeof(T)> {};

    template <typename T>
    inline constexpr size_t size_of_v = size_of<T>::value;

    //- alignment_of
    template <typename T>
    struct alignment_of : std::alignment_of<T> {};

    template <typename T>
    inline constexpr size_t alignment_of_v = alignment_of<T>::value;

    template <typename T, size_t alignment_v = alignment_of_v<T>>
    struct alignment_traits
    {
        using type = std::remove_cvref_t<T>;
        static constexpr size_t type_size = size_of_v<type>;
        static constexpr size_t alignment = std::bit_ceil(alignment_v);
    };
}