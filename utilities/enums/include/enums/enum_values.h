#pragma once

#include "concepts/enum_concept.h"
#include <array>
#include "enum_count.h"

namespace lcf {
    template <enum_c Enum>
    inline constexpr auto enum_values_v = magic_enum::enum_values<Enum>();
}