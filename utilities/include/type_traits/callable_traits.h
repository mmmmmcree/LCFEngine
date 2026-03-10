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
    struct callable_signature<R(C::*)(Args...) const noexcept>
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

    template <typename C, typename R, typename... Args>
    struct callable_signature<R(C::*)(Args...) noexcept>
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
    struct callable_traits;

    template <typename R, typename... Args>
    struct callable_traits<R(*)(Args...)>
    {
        using signature = callable_signature<R(*)(Args...)>;
        using result_type = typename signature::result_type;
        using arg_types = typename signature::arg_types;
    };

    template <typename T, typename R, typename... Args>
    struct callable_traits<R(T::*)(Args...)>
    {
        using signature = callable_signature<R(T::*)(Args...)>;
        using result_type = typename signature::result_type;
        using arg_types = typename signature::arg_types;
    };

    template <typename T, typename R, typename... Args>
    struct callable_traits<R(T::*)(Args...) noexcept>
    {
        using signature = callable_signature<R(T::*)(Args...) noexcept>;
        using result_type = typename signature::result_type;
        using arg_types = typename signature::arg_types;
    };

    template <typename T, typename R, typename... Args>
    struct callable_traits<R(T::*)(Args...) const>
    {
        using signature = callable_signature<R(T::*)(Args...) const>;
        using result_type = typename signature::result_type;
        using arg_types = typename signature::arg_types;
    };

    template <typename T, typename R, typename... Args>
    struct callable_traits<R(T::*)(Args...) const noexcept>
    {
        using signature = callable_signature<R(T::*)(Args...) const noexcept>;
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