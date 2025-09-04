#pragma once

#include <type_traits>

namespace lcf {
    template <typename T>
    concept type_alignment_info_c = requires {
        typename T::type;
        { T::s_type_size } -> std::convertible_to<size_t>;
        { T::s_value } -> std::convertible_to<size_t>;
    };
}