#pragma once

#include "half.hpp"
#include "lcf_type_traits.h"

namespace lcf {
    using float16_t = half_float::half;

    template <>
    struct is_floating_point<float16_t> : std::true_type {};
}