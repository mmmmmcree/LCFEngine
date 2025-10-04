#pragma once

#include <ranges>

namespace lcf {
    template <typename T>
    concept span_convertible_c = std::ranges::contiguous_range<std::remove_cvref_t<T>> and
        std::ranges::sized_range<std::remove_cvref_t<T>>;

    template <typename T>
    concept integral_span_convertible_c = span_convertible_c<T> and
        std::is_integral_v<std::ranges::range_value_t<std::remove_cvref_t<T>>>;
}