#pragma once

#include "concepts/enum_concept.h"
#include "enums/enum_name.h"
#include "enums/enum_values.h"
#include <utility>

namespace lcf {

template <enum_c Enum>
struct enum_basic_traits
{
    template <Enum enum_value>
    inline static constexpr auto index_of_v = std::to_underlying(enum_value);
    template <Enum enum_value>
    inline static auto name_of_v = enum_name<Enum>(enum_value);
    inline static constexpr auto values_v = enum_values_v<Enum>;
};

template <enum_c Enum>
struct enum_traits;

} // namespace lcf