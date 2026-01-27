#pragma once

#include "concepts/enum_concept.h"
#include <magic_enum/magic_enum.hpp>
#include <magic_enum/magic_enum_flags.hpp>

namespace lcf {
    template <enum_c Enum>
    std::string_view enum_name(Enum enum_value) noexcept
    {
        return magic_enum::enum_name(enum_value);
    }

    template <enum_flags_c EnumFlags>
    std::string_view enum_name(EnumFlags enum_flags_value) noexcept
    {
        return magic_enum::enum_flags_name(enum_flags_value);
    }
}