#pragma once

#include <type_traits>

namespace lcf {
    template <typename T>
    concept standard_layout_c = std::is_standard_layout_v<T>;
}