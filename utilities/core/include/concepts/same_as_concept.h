#pragma once

#include <concepts>

namespace lcf {
    template<typename T, typename... Args>
    concept any_same_as_c = (std::same_as<T, Args> || ...);

    template<typename T, typename... Args>
    concept all_same_as_c = (std::same_as<T, Args> && ...);
}