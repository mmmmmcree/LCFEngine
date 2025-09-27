#pragma once

#include <ranges>

namespace lcf {
    template<typename T>
    concept span_convertible_c = std::ranges::contiguous_range<std::remove_cvref_t<T>> and
        std::ranges::sized_range<std::remove_cvref_t<T>>;
}