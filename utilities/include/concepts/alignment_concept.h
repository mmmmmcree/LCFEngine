#pragma once

#include <type_traits>
#include <bit>

namespace lcf {
    template <typename T>
    concept alignment_c = requires {
        typename T::type;
        { T::s_type_size } -> std::convertible_to<size_t>;
        { T::s_alignment } -> std::convertible_to<size_t>;
    };

    template <typename T, size_t alignment_v>
    struct alignment_traits
    {
        using type = std::remove_cvref_t<T>;
        static constexpr size_t s_type_size = sizeof(type);
        static constexpr size_t s_alignment = std::bit_ceil(alignment_v);
    };

    template <typename T>
    using std_alignment_traits = alignment_traits<T, std::alignment_of_v<T>>;
}