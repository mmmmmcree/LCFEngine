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

template <typename T>
using ResultPlaceholder = std::optional<Result<T>>;

template <typename... Values>
using MergedResultViews = std::tuple<ResultView<Values>...>;

inline std::error_code exception_error(std::exception_ptr exception) noexcept
{
    try { std::rethrow_exception(exception); }
    catch (const std::system_error & error) { return error.code(); }
    catch (...) { return std::make_error_code(std::errc::io_error); }
}

template <typename T>
struct result_to_arguments_traits
{
    using type = std::tuple<const T &>;
    static constexpr type get(const Result<T> & result) { return std::tie(result.value()); }
};

template <>
struct result_to_arguments_traits<void>
{
    using type = std::tuple<>;
    static constexpr type get(const Result<void> &) noexcept { return {}; }
};

template <typename T>
using result_to_arguments_t = typename result_to_arguments_traits<T>::type;

template <typename... Values>
using results_to_arguments_t = decltype(std::tuple_cat(std::declval<result_to_arguments_t<Values>>()...));

template <typename T>
constexpr auto result_to_arguments(const Result<T> & result) noexcept(noexcept(result_to_arguments_traits<T>::get(result)))
{
    return result_to_arguments_traits<T>::get(result);
}

template <typename... Values>
struct result_to_arguments_traits<MergedResultViews<Values...>>
{
    using type = results_to_arguments_t<Values...>;
    static constexpr type get(const Result<MergedResultViews<Values...>> & result)
    {
        return std::apply([](const auto &... result_rv) {
            return std::tuple_cat(result_to_arguments(*result_rv)...);
        }, result.value());
    }
};

template <typename T>
struct normalized_value_traits : std::type_identity<T> {};

template <typename T>
struct normalized_value_traits<Result<T>> : std::type_identity<T> {};

template <typename T, typename Executor>
struct normalized_value_traits<asio::awaitable<T, Executor>> : normalized_value_traits<T> {};

template <>
struct normalized_value_traits<std::error_code> : std::type_identity<void> {};

template <typename Return>
using normalized_value_t = typename normalized_value_traits<std::remove_cvref_t<Return>>::type;

template <typename Return>
using normalized_result_t = Result<normalized_value_t<Return>>;

template <typename T>
struct is_result_traits : std::false_type {};

template <typename T>
struct is_result_traits<Result<T>> : std::true_type {};

template <typename T>
using is_result_t = is_result_traits<std::remove_cvref_t<T>>;

template <typename T>
inline constexpr bool is_result_v = is_result_t<T>::value;

template <typename T>
struct is_awaitable_traits : std::false_type {};

template <typename T, typename Executor>
struct is_awaitable_traits<asio::awaitable<T, Executor>> : std::true_type {};

template <typename T>
using is_awaitable_t = is_awaitable_traits<std::remove_cvref_t<T>>;

template <typename T>
inline constexpr bool is_awaitable_v = is_awaitable_t<T>::value;

template <typename T>
using awaitable_value_t = typename std::remove_cvref_t<T>::value_type;

template <typename T>
struct is_unexpected_traits : std::false_type {};

template <typename T>
struct is_unexpected_traits<std::unexpected<T>> : std::true_type {};

template <typename T>
using is_unexpected_t = is_unexpected_traits<std::remove_cvref_t<T>>;

template <typename T>
inline constexpr bool is_unexpected_v = is_unexpected_t<T>::value;

template <typename T>
inline constexpr bool is_error_code_v = std::same_as<std::remove_cvref_t<T>, std::error_code>;

template <typename Return>
inline constexpr bool is_nothrow_result_conversion_v = is_error_code_v<Return>
    or std::is_nothrow_constructible_v<normalized_result_t<Return>, std::add_rvalue_reference_t<Return>>;

template <typename T>
concept result_value_c = std::same_as<T, void> or
    (std::is_object_v<T> and not std::is_array_v<T> and std::move_constructible<T>
    and not is_unexpected_v<T>
    and not std::same_as<std::remove_cv_t<T>, std::in_place_t>
    and not std::same_as<std::remove_cv_t<T>, std::unexpect_t>);

}

template <details::result_value_c T, typename... Arguments>
    requires std::constructible_from<Result<T>, Arguments...>
ResultView<T> make_result_view(Arguments &&... arguments)
{
    return std::make_shared<const Result<T>>(std::forward<Arguments>(arguments)...);
}

template <details::result_value_c T, typename Owner>
ResultView<T> make_result_view(std::shared_ptr<Owner> owner_sp, const Result<T> * result_p) noexcept
{
    return ResultView<T> {std::move(owner_sp), result_p};
}

template <details::result_value_c T> class Chain;
template <details::result_value_c T> class Flow;

namespace details {

template <typename Return>
struct supported_return_traits : std::bool_constant<
    result_value_c<normalized_value_t<Return>> and
    (std::is_void_v<Return> or std::constructible_from<std::remove_cvref_t<Return>, Return>)> {};

template <typename T, typename Executor>
struct supported_return_traits<asio::awaitable<T, Executor>> : std::bool_constant<
    std::same_as<Executor, asio::any_io_executor> and supported_return_traits<T>::value
        and not is_awaitable_v<T>> {};

template <typename Return>
using is_supported_return_t = supported_return_traits<Return>;

template <typename Return>
inline constexpr bool is_supported_return_v = is_supported_return_t<Return>::value;

template <typename Function, typename Arguments>
struct invocation_traits {};

template <typename Function, typename... Arguments>
requires std::invocable<Function &, Arguments...>
struct invocation_traits<Function, std::tuple<Arguments...>>
{
    using return_type = std::invoke_result_t<Function &, Arguments...>;
};

template <typename Function, typename Arguments>
using invocation_return_t = typename invocation_traits<Function, Arguments>::return_type;

template <typename Function, typename Arguments>
using normalized_invocation_value_t = normalized_value_t<invocation_return_t<Function, Arguments>>;

template <typename Function, typename Arguments>
using normalized_invocation_return_t = normalized_result_t<invocation_return_t<Function, Arguments>>;

template <typename Function, typename Arguments>
using invocation_placeholder_t = ResultPlaceholder<normalized_invocation_value_t<Function, Arguments>>;

template <typename Function, typename Arguments>
concept function_c = std::move_constructible<Function>
    and requires { typename invocation_return_t<Function, Arguments>; }
    and is_supported_return_v<invocation_return_t<Function, Arguments>>;

template <typename Function, typename... Inputs>
concept task_function_c = function_c<Function, results_to_arguments_t<Inputs...>>;

template <typename Function, typename... Inputs>
using function_return_t = invocation_return_t<Function, results_to_arguments_t<Inputs...>>;

template <typename Function, typename... Inputs>
using function_value_t = normalized_invocation_value_t<Function, results_to_arguments_t<Inputs...>>;

template <typename Completion, typename Value>
concept completion_c = std::move_constructible<Completion> and std::invocable<Completion &, const Result<Value> &>;

template <typename Return>
constexpr normalized_result_t<Return> as_task_result(Return && result) noexcept(is_nothrow_result_conversion_v<Return>)
{
    if constexpr (is_error_code_v<Return>) {
        if (result) { return std::unexpected(result); }
        return {};
    } else if constexpr (is_result_v<Return>) {
        return std::forward<Return>(result);
    } else {
        return normalized_result_t<Return> {std::in_place, std::forward<Return>(result)};
    }
}

template <typename Function, typename Arguments>
requires function_c<Function, Arguments>
asio::awaitable<invocation_placeholder_t<Function, Arguments>>
invoke_task(Function function, Arguments arguments)
{
    using Return = invocation_return_t<Function, Arguments>;
    co_await asio::post(asio::use_awaitable);
    if constexpr (is_awaitable_v<Return>) {
        if constexpr (std::is_void_v<awaitable_value_t<Return>>) {
            co_await std::apply(function, arguments);
            co_return Result<void> {};
        } else {
            co_return as_task_result(co_await std::apply(function, arguments));
        }
    } else {
        if constexpr (std::is_void_v<Return>) {
            std::apply(function, arguments);
            co_return Result<void> {};
        } else {
            co_return as_task_result(std::apply(function, arguments));
        }
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
