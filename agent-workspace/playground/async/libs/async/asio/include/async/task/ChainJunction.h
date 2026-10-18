#pragma once

#include "Chain.h"
#include <asio/experimental/channel_error.hpp>
#include <asio/experimental/concurrent_channel.hpp>
#include <asio/this_coro.hpp>
#include <atomic>
#include <mutex>
#include <system_error>

namespace lcf::async::task {

template <details::result_value_c... Values>
class ChainJunction
{
    template <details::chain::result_token_c... Tokens>
    requires (sizeof...(Tokens) > 0u)
    friend auto merge(std::pmr::memory_resource * memory_resource_p, Tokens... result_tokens);
    using Self = ChainJunction;
    template <typename Value>
    using ChainResultToken = details::chain::ResultToken<Value>;
    template <typename Value>
    using ChainExecutionState = details::chain::ExecutionState<Value>;
    template <typename Value>
    using ChainOperation = typename ChainExecutionState<Value>::Operation;
    using ResultTokens = std::tuple<ChainResultToken<Values>...>;
    using CompletionChannel = asio::experimental::concurrent_channel<void(std::error_code)>;
    using CompletionChannelHandle = std::shared_ptr<CompletionChannel>;
    struct CompletionState
    {
        CompletionState(std::size_t remaining) noexcept : m_remaining(remaining) {}
        bool isReady() const noexcept { return m_ready.load(std::memory_order_acquire); }
        CompletionChannelHandle subscribe(asio::any_io_executor executor, std::pmr::memory_resource * memory_resource_p)
        {
            std::lock_guard lock {m_mutex};
            if (this->isReady()) { return {}; }
            if (not m_channel_handle) { m_channel_handle = make_completion_channel_handle(std::move(executor), memory_resource_p); }
            return m_channel_handle;
        }
        CompletionChannelHandle complete()
        {
            std::lock_guard lock {m_mutex};
            if (--m_remaining != 0u) { return {}; }
            m_ready.store(true, std::memory_order_release);
            return std::exchange(m_channel_handle, {});
        }
        std::mutex m_mutex;
        std::size_t m_remaining;
        std::atomic_bool m_ready = false;
        CompletionChannelHandle m_channel_handle;
    };
    using CompletionStateHandle = std::shared_ptr<CompletionState>;
public:
    ~ChainJunction() noexcept = default;
    ChainJunction(const Self &) noexcept = default;
    ChainJunction(Self &&) noexcept = default;
    Self & operator=(const Self &) noexcept = default;
    Self & operator=(Self &&) noexcept = default;
public:
    template <typename Function, details::chain::result_token_c... Tokens>
    requires details::task_function_c<Function, Values..., typename Tokens::value_type...>
    auto link(asio::any_io_executor executor, Function function, Tokens... result_tokens) const
    {
        using Value = details::function_value_t<Function, Values..., typename Tokens::value_type...>;
        auto dependency_handles = std::tuple {std::move(result_tokens.m_execution_state_handle)...};
        auto operation = [input_tokens = m_result_tokens, completion_state_handle = m_completion_state_handle,
            dependency_handles = std::move(dependency_handles),
            executor, memory_resource_p = m_memory_resource_p,
            function = std::move(function)](ChainExecutionState<Value> & execution_state) mutable -> asio::awaitable<void> {
            co_await wait(completion_state_handle, memory_resource_p);
            auto failure_opt = std::apply([](const auto &... input_token) {
                std::optional<std::error_code> failure_opt;
                ([&] {
                    const auto & result = input_token.result();
                    if (not failure_opt and not result) { failure_opt = result.error(); }
                }(), ...);
                return failure_opt;
            }, input_tokens);
            if (failure_opt) {
                execution_state.publish(std::unexpected(*failure_opt));
                co_return;
            }
            auto arguments = std::apply([&](const auto &... input_token) {
                return std::apply([&](const auto &... dependency_handle) {
                    assert((... and dependency_handle->isReady()) and
                        "the result referenced by an additional ResultToken must already be completed; "
                        "put independent unfinished tasks in merge() to wait for their results");
                    return details::merge_results_to_arguments(input_token.result()..., dependency_handle->result()...);
                }, dependency_handles);
            }, input_tokens);
            execution_state.publish(co_await details::spawn_result(executor, details::invoke_task(std::move(function), std::move(arguments))));
        };
        auto execution_state_handle = Chain<Value>::make_execution_state_handle(std::move(operation), m_memory_resource_p);
        return Chain<Value> {std::move(execution_state_handle), std::move(executor), m_memory_resource_p};
    }
    template <typename Function, typename Handler, details::chain::result_token_c... Tokens>
    requires details::task_error_handler_c<Handler, Function, Values..., typename Tokens::value_type...>
    auto link(asio::any_io_executor executor, Function function, ErrorHandler<Handler> error_handler, Tokens... result_tokens) const
    {
        return this->link(std::move(executor),
            details::ErrorHandledTask {std::move(function), std::move(error_handler.m_function)},
            std::move(result_tokens)...);
    }
    std::pmr::memory_resource * getMemoryResource() const noexcept { return m_memory_resource_p; }
private:
    ChainJunction(std::pmr::memory_resource * memory_resource_p, ChainResultToken<Values>... result_tokens) :
        m_result_tokens(std::move(result_tokens)...),
        m_memory_resource_p(memory_resource_p)
    {
        std::apply([&](const auto &... input_token) {
            const auto remaining = (std::size_t {0} + ... + (not input_token.isReady()));
            if (remaining == 0u) { return; }
            m_completion_state_handle = make_completion_state_handle(remaining, memory_resource_p);
            (observe(input_token, m_completion_state_handle), ...);
        }, m_result_tokens);
    }
private:
    static CompletionStateHandle make_completion_state_handle(std::size_t remaining, std::pmr::memory_resource * memory_resource_p)
    {
        return std::allocate_shared<CompletionState>(std::pmr::polymorphic_allocator<CompletionState> {memory_resource_p}, remaining);
    }
    static CompletionChannelHandle make_completion_channel_handle(asio::any_io_executor executor, std::pmr::memory_resource * memory_resource_p)
    {
        return std::allocate_shared<CompletionChannel>(std::pmr::polymorphic_allocator<CompletionChannel> {memory_resource_p}, std::move(executor), 0u);
    }
    template <details::chain::result_token_c Token>
    static void observe(const Token & result_token, const CompletionStateHandle & completion_state_handle)
    {
        using Value = typename Token::value_type;
        auto & execution_state = *result_token.m_execution_state_handle;
        if (execution_state.isReady()) { return; }
        assert(execution_state.m_operation and "merge() must be constructed before its unfinished source starts executing; "
            "completion notifications cannot be attached to an operation that has already been consumed");
        execution_state.m_operation = [operation = std::move(execution_state.m_operation), completion_state_handle]
            (ChainExecutionState<Value> & execution_state) mutable {
            return execute_and_notify<Value>(std::move(operation), execution_state, completion_state_handle);
        };
    }
    template <typename Value>
    static asio::awaitable<void> execute_and_notify(
        ChainOperation<Value> operation,
        ChainExecutionState<Value> & execution_state,
        CompletionStateHandle completion_state_handle)
    {
        try {
            co_await operation(execution_state);
        } catch (...) {
            execution_state.publish(std::unexpected(details::exception_error(std::current_exception())));
        }
        operation = {};
        auto channel_handle = completion_state_handle->complete();
        if (channel_handle) { channel_handle->close(); }
    }
    static asio::awaitable<void> wait(CompletionStateHandle completion_state_handle, std::pmr::memory_resource * memory_resource_p)
    {
        if (not completion_state_handle or completion_state_handle->isReady()) { co_return; }
        auto channel_handle = completion_state_handle->subscribe(co_await asio::this_coro::executor, memory_resource_p);
        if (not channel_handle) { co_return; }
        try {
            co_await channel_handle->async_receive(asio::use_awaitable);
        } catch (const std::system_error & exception) {
            if (exception.code() != asio::experimental::error::channel_closed) { throw; }
        }
    }
private:
    ResultTokens m_result_tokens;
    CompletionStateHandle m_completion_state_handle;
    std::pmr::memory_resource * m_memory_resource_p;
};

template <details::chain::result_token_c... Tokens>
requires (sizeof...(Tokens) > 0u)
auto merge(std::pmr::memory_resource * memory_resource_p, Tokens... result_tokens)
{
    return ChainJunction<typename Tokens::value_type...> {memory_resource_p, std::move(result_tokens)...};
}

template <details::chain::result_token_c... Tokens>
requires (sizeof...(Tokens) > 0u)
auto merge(Tokens... result_tokens)
{
    return merge(std::pmr::get_default_resource(), std::move(result_tokens)...);
}

}
