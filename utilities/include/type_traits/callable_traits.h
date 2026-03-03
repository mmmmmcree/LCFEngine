#pragma once

#include <functional>
#include <tuple>

namespace lcf {
    template <typename>
    struct callable_signature;

    template <typename R, typename... Args>
    struct callable_signature<R(*)(Args...)>
    {
        using result_type = R;
        using arg_types = std::tuple<Args...>;
    };

    template <typename C, typename R, typename... Args>
    struct callable_signature<R(C::*)(Args...) const>
    {
        using result_type = R;
        using arg_types = std::tuple<Args...>;
    };

    template <typename C, typename R, typename... Args>
    struct callable_signature<R(C::*)(Args...)>
    {
        using result_type = R;
        using arg_types = std::tuple<Args...>;
    };

    template <typename R, typename... Args>
    struct callable_signature<std::function<R(Args...)>>
    {
        using result_type = R;
        using arg_types = std::tuple<Args...>;
    };

    template <typename T>
    struct callable_traits
    {
        using signature = callable_signature<decltype(&T::operator())>;
        using result_type = typename signature::result_type;
        using arg_types = typename signature::arg_types;
    };

    template <typename R, typename... Args>
    struct callable_traits<R(*)(Args...)>
    {
        using signature = callable_signature<R(*)(Args...)>;
        using result_type = typename signature::result_type;
        using arg_types = typename signature::arg_types;
    };

    template <typename R, typename... Args>
    struct callable_traits<std::function<R(Args...)>>
    {
        using signature = callable_signature<std::function<R(Args...)>>;
        using result_type = typename signature::result_type;
        using arg_types = typename signature::arg_types;
    };
}