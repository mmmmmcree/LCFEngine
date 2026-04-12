#pragma once

#include <concepts>
#include <ranges>

namespace lcf {
    template <typename Range, typename T>
    concept range_of_c = std::ranges::range<Range> and
        std::same_as<std::ranges::range_value_t<Range>, T>;

    template <typename Range, typename T>
    concept convertible_range_of_c = std::ranges::range<Range> and
        std::convertible_to<std::ranges::range_value_t<Range>, T>;
}
