#pragma once

#include <magic_enum/magic_enum.hpp>
#include "concepts/enum_concept.h"

namespace lcf {
    template <enum_c Enum>
    inline constexpr uint32_t enum_count_v = static_cast<uint32_t>(magic_enum::enum_count<Enum>());
}