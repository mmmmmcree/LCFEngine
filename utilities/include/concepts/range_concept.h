#pragma once

#include <concepts>
#include <ranges>

namespace lcf {
    template <typename Range, typename T>
    concept compatible_range_c = std::ranges::input_range<Range> and
        std::convertible_to<std::ranges::range_reference_t<Range>, T>;
}
