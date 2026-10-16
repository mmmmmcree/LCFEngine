#pragma once

#include "common.h"
#include <asio/steady_timer.hpp>
#include <asio/this_coro.hpp>
#include <chrono>
#include <cstddef>
#include <variant>

namespace lcf::async::task {

class ErrorContext
{
public:
    ErrorContext(std::error_code error, std::size_t attempt) noexcept : m_error(error), m_attempt_count(attempt) {}
public:
    std::error_code getErrorCode() const noexcept { return m_error; }
    std::size_t getAttemptCount() const noexcept { return m_attempt_count; }
private:
    std::error_code m_error;
    std::size_t m_attempt_count;
};

struct RetryAfter
{
    std::chrono::steady_clock::duration m_delay {};
};

template <details::result_value_c T>
using ErrorAction = std::variant<Result<T>, RetryAfter>;

template <typename Handler>
struct ErrorHandler
{
    Handler m_function;
};

template <std::move_constructible Handler>
auto on_error(Handler handler) noexcept(std::is_nothrow_move_constructible_v<Handler>)
{
    return ErrorHandler<Handler> {std::move(handler)};
}

inline std::unexpected<std::error_code> propagate(std::error_code error) noexcept { return std::unexpected(error); }

inline RetryAfter retry_after(std::chrono::steady_clock::duration delay) noexcept { return {delay}; }

template <typename Value>
requires (details::result_value_c<std::remove_cvref_t<Value>>
    and std::constructible_from<std::remove_cvref_t<Value>, Value>)
Result<std::remove_cvref_t<Value>> use_value(Value && value)
    noexcept(std::is_nothrow_constructible_v<Result<std::remove_cvref_t<Value>>, std::in_place_t, Value>)
{
    return Result<std::remove_cvref_t<Value>> {std::in_place, std::forward<Value>(value)};
}

inline Result<void> use_value() noexcept { return {}; }

namespace details {

template <typename Return, typename Value>
struct supported_error_handler_return_traits : std::bool_constant<std::constructible_from<ErrorAction<Value>, Return>> {};

template <typename Return, typename Executor, typename Value>
struct supported_error_handler_return_traits<asio::awaitable<Return, Executor>, Value> : std::bool_constant<
    std::same_as<Executor, asio::any_io_executor> and std::constructible_from<ErrorAction<Value>, Return>> {};

template <typename Return, typename Value>
inline constexpr bool is_supported_error_handler_return_v = supported_error_handler_return_traits<Return, Value>::value;

template <typename Handler>
using error_handler_return_t = invocation_return_t<Handler, std::tuple<const ErrorContext &>>;

template <typename Handler, typename Value>
concept error_handler_c = std::move_constructible<Handler>
    and std::invocable<Handler &, const ErrorContext &>
    and is_supported_error_handler_return_v<error_handler_return_t<Handler>, Value>;

template <typename Value, typename Handler>
asio::awaitable<ErrorAction<Value>> invoke_error_handler(Handler & handler, const ErrorContext & context)
{
    if constexpr (is_awaitable_v<error_handler_return_t<Handler>>) {
        co_return ErrorAction<Value> {co_await std::invoke(handler, context)};
    } else {
        co_return ErrorAction<Value> {std::invoke(handler, context)};
    }
}

template <typename Function, typename Handler, typename Arguments>
asio::awaitable<normalized_invocation_return_t<Function, Arguments>>
invoke_task_with_error_handler(Function function, Handler handler, Arguments arguments)
{
    using Value = normalized_invocation_value_t<Function, Arguments>;
    for (std::size_t attempt = 1; ; ++attempt) {
        auto result_placeholder = co_await invoke_task(std::ref(function), arguments);
        if (*result_placeholder) { co_return std::move(*result_placeholder); }
        const ErrorContext context {result_placeholder->error(), attempt};
        auto action = co_await invoke_error_handler<Value>(handler, context);
        if (const auto * retry_p = std::get_if<RetryAfter>(&action)) {
            if (retry_p->m_delay > std::chrono::steady_clock::duration::zero()) {
                asio::steady_timer timer {co_await asio::this_coro::executor};
                timer.expires_after(retry_p->m_delay);
                co_await timer.async_wait(asio::use_awaitable);
            }
            continue;
        }
        co_return std::move(std::get<Result<Value>>(action));
    }
}

template <typename Function, typename Handler>
class ErrorHandledTask
{
public:
    ErrorHandledTask(Function function, Handler handler) :
        m_function(std::move(function)), m_handler(std::move(handler)) {}
public:
    template <typename... Arguments>
    requires (function_c<Function, std::tuple<Arguments...>>
        and error_handler_c<Handler, normalized_invocation_value_t<Function, std::tuple<Arguments...>>>)
    auto operator()(Arguments &&... arguments) -> asio::awaitable<normalized_invocation_return_t<Function, std::tuple<Arguments...>>>
    {
        return invoke_task_with_error_handler(std::ref(m_function), std::ref(m_handler),
            std::tuple<Arguments...> {std::forward<Arguments>(arguments)...});
    }
private:
    Function m_function;
    Handler m_handler;
};

}

}
