#pragma once

#include <type_traits>

namespace lcf {
    template <typename T>
    concept number_c = std::is_arithmetic_v<T>;
}