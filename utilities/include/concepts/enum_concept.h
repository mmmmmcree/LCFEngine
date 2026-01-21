#pragma once

#include "type_traits/enum_traits.h"

namespace lcf {
    template <typename T>
    concept enum_c = std::is_enum_v<T>;

    template <typename T>
    concept enum_flags_c = std::is_enum_v<T> and is_enum_flags_v<T>;
}