#pragma once

#include <type_traits>

namespace lcf {
    template <typename T>
    concept alignment_c = requires {
        typename T::type;
        { T::type_size } -> std::convertible_to<size_t>;
        { T::alignment } -> std::convertible_to<size_t>;
    };
}