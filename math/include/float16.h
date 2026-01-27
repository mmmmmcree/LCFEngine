#pragma once

#include "half.hpp"
#include "lcf_type_traits.h"

namespace lcf {
    using float16_t = half_float::half;

    template <> inline constexpr bool is_floating_point_v<float16_t> = true;
}