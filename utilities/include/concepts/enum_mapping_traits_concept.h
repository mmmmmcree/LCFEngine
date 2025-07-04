#pragma once

#include <type_traits>
#include <ranges>

namespace lcf {
    template <typename T>
    concept EnumMappingTraitsConcept = requires {
        typename T::mapping_type;
        { T::mappings } -> std::ranges::range;
        { *std::begin(T::mappings) } -> std::convertible_to<typename T::mapping_type>;
    };
}