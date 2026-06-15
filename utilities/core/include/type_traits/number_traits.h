#pragma once

#include "float16.h"

namespace lcf {
    template <typename Number, size_t bytes>
    struct number_traits;

    template <typename Number, size_t bytes>
    using number_t = typename number_traits<Number, bytes>::type;

    template <> struct number_traits<int, 1> { using type = int8_t; };
    template <> struct number_traits<int, 2> { using type = int16_t; };
    template <> struct number_traits<int, 4> { using type = int32_t; };
    template <> struct number_traits<int, 8> { using type = int64_t; };

    template <> struct number_traits<unsigned int, 1> { using type = uint8_t; };
    template <> struct number_traits<unsigned int, 2> { using type = uint16_t; };
    template <> struct number_traits<unsigned int, 4> { using type = uint32_t; };
    template <> struct number_traits<unsigned int, 8> { using type = uint64_t; };

    template <> struct number_traits<float, 2> { using type = float16_t; };
    template <> struct number_traits<float, 4> { using type = std::float_t; };
    template <> struct number_traits<float, 8> { using type = std::double_t; };
}