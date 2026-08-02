#pragma once

#include "type_traits/lcf_type_traits.h"

namespace lcf {
    template <typename T>
    concept enum_c = std::is_enum_v<T>;

    template <typename T>
    concept enum_flags_c = enum_c<T> and is_enum_flags_v<T>;
}