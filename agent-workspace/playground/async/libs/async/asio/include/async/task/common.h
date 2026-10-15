#pragma once

#include <mutex>
#include <asio/any_io_executor.hpp>
#include <asio/awaitable.hpp>
#include <asio/post.hpp>
#include <asio/use_awaitable.hpp>
#include <concepts>
#include <cstdint>
#include <exception>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <utility>

namespace lcf::async::task {

template <typename T>
using Result = std::expected<T, std::error_code>;

template <typename T>
using ResultView = std::shared_ptr<const Result<T>>;

enum class State : std::uint8_t
{
    ePending,
    eRunning,
    eCompleted
};

namespace details {

struct FlowAccess;

inline std::error_code exception_error(std::exception_ptr exception) noexcept
{
    try { std::rethrow_exception(exception); }
    catch (const std::system_error & error) { return error.code(); }
    catch (...) { return std::make_error_code(std::errc::io_error); }
}

template <typename... Values>
struct MergedValues
{
    using ResultTuple = std::tuple<ResultView<Values>...>;
    ResultTuple m_results;
};

template <typename T>
struct result_arguments_traits
{
    using type = std::tuple<const T &>;
    static type get(const Result<T> & result) { return std::tie(result.value()); }
};

template <>
struct result_arguments_traits<void>
{
    using type = std::tuple<>;
    static type get(const Result<void> &) { return {}; }
};

template <typename T>
auto result_arguments(const Result<T> & result)
{
    return result_arguments_traits<T>::get(result);
}

template <typename... Values>
struct result_arguments_traits<MergedValues<Values...>>
{
    using type = decltype(std::tuple_cat(std::declval<typename result_arguments_traits<Values>::type>()...));
    static type get(const Result<MergedValues<Values...>> & result)
    {
        return std::apply([](const auto &... result_sp) {
            return std::tuple_cat(result_arguments(*result_sp)...);
        }, result.value().m_results);
    }
};

template <typename... Values>
using arguments_t = decltype(std::tuple_cat(std::declval<typename result_arguments_traits<Values>::type>()...));

template <typename T>
struct task_value_traits
{
    using type = T;
};

template <typename T>
struct task_value_traits<Result<T>>
{
    using type = T;
};

template <typename T, typename Executor>
struct task_value_traits<asio::awaitable<T, Executor>> : task_value_traits<T> {};

template <>
struct task_value_traits<std::error_code>
{
    using type = void;
};

template <typename T>
using task_value_t = typename task_value_traits<std::remove_cvref_t<T>>::type;

template <typename T>
struct is_task_result_traits : std::false_type {};

template <typename T>
struct is_task_result_traits<Result<T>> : std::true_type {};

template <typename T>
struct is_awaitable_traits : std::false_type {};

template <typename T, typename Executor>
struct is_awaitable_traits<asio::awaitable<T, Executor>> : std::true_type {};

template <typename T>
struct is_unexpected_traits : std::false_type {};

template <typename T>
struct is_unexpected_traits<std::unexpected<T>> : std::true_type {};

template <typename T>
concept value_c = std::same_as<T, void> or
    (std::is_object_v<T> and not std::is_array_v<T> and std::move_constructible<T>
        and not is_unexpected_traits<std::remove_cv_t<T>>::value
        and not std::same_as<std::remove_cv_t<T>, std::in_place_t>
        and not std::same_as<std::remove_cv_t<T>, std::unexpect_t>);

}

template <details::value_c T> class Chain;
template <details::value_c T> class Flow;

namespace details {

template <typename Return>
struct supported_return_traits : std::bool_constant<
    value_c<task_value_t<Return>> and
    (std::is_void_v<Return> or std::constructible_from<std::remove_cvref_t<Return>, Return>)> {};

template <typename T, typename Executor>
struct supported_return_traits<asio::awaitable<T, Executor>> : std::bool_constant<
    std::same_as<Executor, asio::any_io_executor> and supported_return_traits<T>::value
        and not is_awaitable_traits<T>::value> {};

template <typename Function, typename Arguments>
struct invocation_traits {};

template <typename Function, typename... Arguments>
    requires std::invocable<Function &, Arguments...>
struct invocation_traits<Function, std::tuple<Arguments...>>
{
    using return_type = std::invoke_result_t<Function &, Arguments...>;
};

template <typename Function, typename Arguments>
concept function_c = std::move_constructible<Function>
    and requires { typename invocation_traits<Function, Arguments>::return_type; }
    and supported_return_traits<typename invocation_traits<Function, Arguments>::return_type>::value;

template <typename Function, typename... Inputs>
concept task_function_c = function_c<Function, arguments_t<Inputs...>>;

template <typename Function, typename... Inputs>
using function_return_t = typename invocation_traits<Function, arguments_t<Inputs...>>::return_type;

template <typename Completion, typename Value>
concept completion_c = std::move_constructible<Completion> and std::invocable<Completion &, const Result<Value> &>;

template <typename Return>
Result<task_value_t<Return>> as_task_result(Return result)
{
    using Value = task_value_t<Return>;
    if constexpr (std::same_as<Return, std::error_code>) {
        if (result) { return std::unexpected(result); }
        return {};
    } else if constexpr (is_task_result_traits<Return>::value) {
        return result;
    } else {
        return Result<Value> {std::in_place, std::move(result)};
    }
}

template <typename Function, typename Arguments>
    requires function_c<Function, Arguments>
asio::awaitable<std::optional<Result<task_value_t<typename invocation_traits<Function, Arguments>::return_type>>>>
invoke_task(Function function, Arguments arguments)
{
    using Return = typename invocation_traits<Function, Arguments>::return_type;
    co_await asio::post(asio::use_awaitable);
    if constexpr (is_awaitable_traits<std::remove_cvref_t<Return>>::value) {
        if constexpr (std::is_void_v<typename Return::value_type>) {
            co_await std::apply(function, arguments);
            co_return Result<void> {};
        } else {
            co_return as_task_result(co_await std::apply(function, arguments));
        }
    } else if constexpr (std::is_void_v<Return>) {
        std::apply(function, arguments);
        co_return Result<void> {};
    } else {
        co_return as_task_result(std::apply(function, arguments));
    }
}

template <typename T>
struct source_traits {};

template <typename T>
struct source_traits<Chain<T>>
{
    using value_type = T;
    static constexpr bool k_shared = false;
};

template <typename T>
struct source_traits<Flow<T>>
{
    using value_type = T;
    static constexpr bool k_shared = true;
};

template <typename T>
concept source_c = not std::is_volatile_v<std::remove_reference_t<T>>
    and requires { typename source_traits<std::remove_cvref_t<T>>::value_type; }
    and (source_traits<std::remove_cvref_t<T>>::k_shared
        or (not std::is_lvalue_reference_v<T> and not std::is_const_v<std::remove_reference_t<T>>));

template <typename T>
using source_value_t = typename source_traits<std::remove_cvref_t<T>>::value_type;

}

}
