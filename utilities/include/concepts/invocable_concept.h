#pragma once

#include <concepts>

namespace lcf {
    template <typename Invocable, typename... Args>
    concept invocable_c = std::invocable<Invocable, Args...>;

    //- constraint return type invocable concept
    template <typename Invocable, typename T, typename... Args>
    concept crt_invocable_c = std::invocable<Invocable, Args...> &&
        std::same_as<std::invoke_result_t<Invocable, Args...>, T>;
}