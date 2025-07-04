#pragma once

#include <type_traits>

namespace lcf {
    template <typename T>
    concept EnumConcept = std::is_enum_v<T>;
}