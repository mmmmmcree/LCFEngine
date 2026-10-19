#pragma once

#include "chain_details.h"
#include "error_handling.h"
#include "execution.h"
#include <cassert>
#include <memory_resource>

namespace lcf::async::task {

LCF_ASYNC_EXPORT
template <details::result_value_c T>
class Chain
{
    template <details::result_value_c> friend class Chain;
    template <details::result_value_c...> friend class ChainJunction;
    using Self = Chain;
    using ExecutionState = details::chain::ExecutionState<T>;
    using ExecutionStateHandle = details::chain::ExecutionStateHandle<T>;
    using Operation = typename ExecutionState::Operation;
public:
    using value_type = T;
    using ResultToken = details::chain::ResultToken<T>;
public:
    ~Chain() noexcept = default;
    template <typename Value>
    requires (not std::is_void_v<T> and std::same_as<std::remove_cvref_t<Value>, T> and std::constructible_from<T, Value>)
    Chain(
        Value && initial_value,
        Executor executor,
        std::pmr::memory_resource * memory_resource_p = std::pmr::get_default_resource()) :
        m_execution_state_handle(make_execution_state_handle(memory_resource_p)),
        m_executor(std::move(executor)),
        m_memory_resource_p(memory_resource_p)
    {
        m_execution_state_handle->publish(Result<T> {std::in_place, std::forward<Value>(initial_value)});
    }
    explicit Chain(
        Executor executor,
        std::pmr::memory_resource * memory_resource_p = std::pmr::get_default_resource()) requires std::is_void_v<T> :
        m_execution_state_handle(make_execution_state_handle(memory_resource_p)),
        m_executor(std::move(executor)),
        m_memory_resource_p(memory_resource_p)
    {
        m_execution_state_handle->publish(Result<void> {});
    }
    Chain(const Self &) = delete;
    Chain(Self &&) noexcept = default;
    Self & operator=(const Self &) = delete;
    Self & operator=(Self &&) noexcept = default;
public:
    template <typename Function, details::chain::result_token_c... Tokens>
    requires details::task_function_c<Function, T, typename Tokens::value_type...>
    auto link(Function function, Tokens... result_tokens) &&
    {
        return std::move(*this).link(m_executor, std::move(function), std::move(result_tokens)...);
    }
    template <typename Function, details::chain::result_token_c... Tokens>
    requires details::task_function_c<Function, T, typename Tokens::value_type...>
    auto link(Executor executor, Function function, Tokens... result_tokens) &&
    {
        using Value = details::function_value_t<Function, T, typename Tokens::value_type...>;
        auto input_execution_state_handle = std::move(m_execution_state_handle);
        auto dependency_handles = std::tuple {std::move(result_tokens.m_execution_state_handle)...};
        auto operation = [input_execution_state_handle = std::move(input_execution_state_handle),
            dependency_handles = std::move(dependency_handles),
            executor,
            function = std::move(function)](typename Chain<Value>::ExecutionState & execution_state, Executor control_executor) mutable -> Awaitable<void> {
            co_await execute(input_execution_state_handle, control_executor);
            co_await ::lcf::async::execution::details::YieldAwaitable {};
            const auto & input_result = input_execution_state_handle->result();
            if (not input_result) {
                execution_state.publish(std::unexpected(input_result.error()));
                co_return;
            }
            auto arguments = std::apply([&](const auto &... dependency_handle) {
                assert((... and dependency_handle->isReady()) and
                    "the result referenced by a ResultToken must already be completed; link() only waits for its direct predecessor, "
                    "so tokens from independent unfinished tasks cannot be used as inputs");
                return details::merge_results_to_arguments(input_result, dependency_handle->result()...);
            }, dependency_handles);
            execution_state.publish(co_await details::spawn_result(executor, details::invoke_task(function, arguments)));
        };
        auto execution_state_handle = Chain<Value>::make_execution_state_handle(std::move(operation), m_memory_resource_p);
        return Chain<Value> {std::move(execution_state_handle), std::move(executor), m_memory_resource_p};
    }
    template <typename Function, typename Handler, details::chain::result_token_c... Tokens>
    requires details::task_error_handler_c<Handler, Function, T, typename Tokens::value_type...>
    auto link(Function function, ErrorHandler<Handler> error_handler, Tokens... result_tokens) &&
    {
        return std::move(*this).link(m_executor, std::move(function), std::move(error_handler), std::move(result_tokens)...);
    }
    template <typename Function, typename Handler, details::chain::result_token_c... Tokens>
    requires details::task_error_handler_c<Handler, Function, T, typename Tokens::value_type...>
    auto link(Executor executor, Function function, ErrorHandler<Handler> error_handler, Tokens... result_tokens) &&
    {
        return std::move(*this).link(
            std::move(executor),
            details::ErrorHandledTask {std::move(function), std::move(error_handler.m_function), m_memory_resource_p},
            std::move(result_tokens)...);
    }
    template <typename Completion>
    requires details::completion_c<Completion, T>
    void start(Executor executor, Completion completion) &&
    {
        auto state_handle = std::move(m_execution_state_handle);
        assert(state_handle and "a consumed or moved-from Chain cannot be executed; "
            "link() and start() transfer ownership from the source Chain");
        state_handle->m_publication.setExecutor(executor);
        auto operation = execute_result(state_handle, executor);
        details::start_owned<T>(std::move(executor), std::move(operation), std::move(state_handle), std::move(completion));
    }
    ResultToken getResultToken() const noexcept { return ResultToken {m_execution_state_handle}; }
    Executor getExecutor() const noexcept { return m_executor; }
    std::pmr::memory_resource * getMemoryResource() const noexcept { return m_memory_resource_p; }
private:
    Chain(ExecutionStateHandle execution_state_handle, Executor executor,
        std::pmr::memory_resource * memory_resource_p) noexcept :
        m_execution_state_handle(std::move(execution_state_handle)),
        m_executor(std::move(executor)), m_memory_resource_p(memory_resource_p) {}
private:
    static ExecutionStateHandle make_execution_state_handle(std::pmr::memory_resource * memory_resource_p)
    {
        return std::allocate_shared<ExecutionState>(std::pmr::polymorphic_allocator<ExecutionState> {memory_resource_p});
    }
    static ExecutionStateHandle make_execution_state_handle(Operation operation, std::pmr::memory_resource * memory_resource_p)
    {
        return std::allocate_shared<ExecutionState>(std::pmr::polymorphic_allocator<ExecutionState> {memory_resource_p}, std::move(operation));
    }
    static details::chain::ExecutionSender<T> execute(ExecutionStateHandle execution_state_handle, Executor control_executor)
    {
        assert(execution_state_handle and "a consumed or moved-from Chain cannot be executed; "
            "link() and start() transfer ownership from the source Chain");
        execution_state_handle->m_publication.setExecutor(control_executor);
        return details::chain::ExecutionSender<T> {std::move(execution_state_handle), std::move(control_executor)};
    }
    static Awaitable<ResultHandle<T>> execute_result(ExecutionStateHandle execution_state_handle, Executor control_executor)
    {
        co_await execute(execution_state_handle, std::move(control_executor));
        const auto * result_p = &execution_state_handle->result();
        co_return make_result_handle(std::move(execution_state_handle), result_p);
    }
private:
    ExecutionStateHandle m_execution_state_handle;
    Executor m_executor;
    std::pmr::memory_resource * m_memory_resource_p;
};

LCF_ASYNC_EXPORT
template <typename T>
Chain(T, Executor, std::pmr::memory_resource * = std::pmr::get_default_resource()) -> Chain<T>;

LCF_ASYNC_EXPORT
Chain(Executor, std::pmr::memory_resource * = std::pmr::get_default_resource()) -> Chain<void>;

}
