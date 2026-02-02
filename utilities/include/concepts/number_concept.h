#pragma once

#include "type_traits/lcf_type_traits.h"

namespace lcf {
    template <typename T>
    concept number_c = is_arithmetic_v<T>;
}