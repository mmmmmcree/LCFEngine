#pragma once

#include <type_traits>

namespace lcf {
    template <typename T>
    concept trivially_copyable_c = std::is_trivially_copyable_v<T>;
}
