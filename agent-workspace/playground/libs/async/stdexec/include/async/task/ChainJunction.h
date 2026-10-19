#pragma once

#include "Chain.h"
#include "notification.h"
#include <atomic>
#include <mutex>
#include <system_error>

namespace lcf::async::task {

LCF_ASYNC_EXPORT
template <details::chain::result_token_c... Tokens>
requires (sizeof...(Tokens) > 0u)
auto merge(std::pmr::memory_resource * memory_resource_p, Tokens... result_tokens);

LCF_ASYNC_EXPORT
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
    using CompletionState = details::Notification;
    using CompletionStateHandle = std::shared_ptr<CompletionState>;
    class SourceObserver final : public details::NotificationWaiter, public ::lcf::async::execution::details::Callback
    {
        using Self = SourceObserver;
    public:
        SourceObserver(const std::shared_ptr<details::Notification> & source_sp, CompletionStateHandle completion_sp) noexcept :
            details::NotificationWaiter {&Self::finish, &Self::dispatch},
            ::lcf::async::execution::details::Callback {&Self::invoke}, m_source_wp(source_sp), m_completion_sp(std::move(completion_sp)) {}
    private:
        static void finish(details::NotificationWaiter * waiter_p, std::exception_ptr) noexcept
        {
            static_cast<Self *>(waiter_p)->m_completion_sp->complete();
        }
        static bool dispatch(std::shared_ptr<details::NotificationWaiter> waiter_sp) noexcept
        {
            auto self_sp = std::static_pointer_cast<Self>(std::move(waiter_sp));
            auto source_sp = self_sp->m_source_wp.lock();
            auto executor_opt = source_sp->getExecutor();
            if (not executor_opt) { self_sp->m_completion_sp->complete(); return false; }
            auto * self_p = self_sp.get();
            self_p->m_queued_source_sp = std::move(source_sp);
            self_p->m_queued_owner_sp = std::move(self_sp);
            executor_opt->enqueue(self_p);
            return true;
        }
        static void invoke(::lcf::async::execution::details::Callback * callback_p) noexcept
        {
            auto * self_p = static_cast<Self *>(callback_p);
            auto self_sp = std::move(self_p->m_queued_owner_sp);
            auto source_sp = std::move(self_sp->m_queued_source_sp);
            self_sp->m_completion_sp->complete();
            source_sp->dispatchNext();
        }
    private:
        std::weak_ptr<details::Notification> m_source_wp;
        CompletionStateHandle m_completion_sp;
        std::shared_ptr<details::Notification> m_queued_source_sp;
        std::shared_ptr<Self> m_queued_owner_sp;
    };
public:
    ~ChainJunction() noexcept = default;
    ChainJunction(const Self &) noexcept = default;
    ChainJunction(Self &&) noexcept = default;
    Self & operator=(const Self &) noexcept = default;
    Self & operator=(Self &&) noexcept = default;
public:
    template <typename Function, details::chain::result_token_c... Tokens>
    requires details::task_function_c<Function, Values..., typename Tokens::value_type...>
    auto link(Executor executor, Function function, Tokens... result_tokens) const
    {
        using Value = details::function_value_t<Function, Values..., typename Tokens::value_type...>;
        auto dependency_handles = std::tuple {std::move(result_tokens.m_execution_state_handle)...};
        auto operation = [input_tokens = m_result_tokens, completion_state_handle = m_completion_state_handle,
            dependency_handles = std::move(dependency_handles),
            executor, memory_resource_p = m_memory_resource_p,
            function = std::move(function)](ChainExecutionState<Value> & execution_state, Executor control_executor) mutable -> Awaitable<void> {
            co_await wait(completion_state_handle, memory_resource_p, std::move(control_executor));
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
            execution_state.publish(co_await details::spawn_result(executor, details::invoke_task(function, arguments)));
        };
        auto execution_state_handle = Chain<Value>::make_execution_state_handle(std::move(operation), m_memory_resource_p);
        return Chain<Value> {std::move(execution_state_handle), std::move(executor), m_memory_resource_p};
    }
    template <typename Function, typename Handler, details::chain::result_token_c... Tokens>
    requires details::task_error_handler_c<Handler, Function, Values..., typename Tokens::value_type...>
    auto link(Executor executor, Function function, ErrorHandler<Handler> error_handler, Tokens... result_tokens) const
    {
        return this->link(std::move(executor),
            details::ErrorHandledTask {std::move(function), std::move(error_handler.m_function), m_memory_resource_p},
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
            (observe(input_token, m_completion_state_handle, memory_resource_p), ...);
        }, m_result_tokens);
    }
private:
    static CompletionStateHandle make_completion_state_handle(std::size_t remaining, std::pmr::memory_resource * memory_resource_p)
    {
        return std::allocate_shared<CompletionState>(std::pmr::polymorphic_allocator<CompletionState> {memory_resource_p}, remaining, memory_resource_p);
    }
    template <details::chain::result_token_c Token>
    static void observe(const Token & result_token, const CompletionStateHandle & completion_state_handle,
        std::pmr::memory_resource * resource_p)
    {
        using Value = typename Token::value_type;
        auto & execution_state = *result_token.m_execution_state_handle;
        if (execution_state.isReady()) { return; }
        assert(execution_state.m_operation and "merge() must be constructed before its unfinished source starts executing; "
            "completion notifications cannot be attached to an operation that has already been consumed");
        auto publication_sp = std::shared_ptr<details::Notification> {result_token.m_execution_state_handle, &execution_state.m_publication};
        auto observer_sp = std::allocate_shared<SourceObserver>(std::pmr::polymorphic_allocator<SourceObserver> {resource_p},
            publication_sp, completion_state_handle);
        execution_state.m_publication.subscribe(std::move(observer_sp));
    }
    static Awaitable<void> wait(CompletionStateHandle completion_state_handle, std::pmr::memory_resource * memory_resource_p,
        Executor control_executor)
    {
        if (not completion_state_handle or completion_state_handle->isReady()) { co_return; }
        co_await details::NotificationSender {std::move(completion_state_handle), memory_resource_p, std::move(control_executor)};
    }
private:
    ResultTokens m_result_tokens;
    CompletionStateHandle m_completion_state_handle;
    std::pmr::memory_resource * m_memory_resource_p;
};

LCF_ASYNC_EXPORT
template <details::chain::result_token_c... Tokens>
requires (sizeof...(Tokens) > 0u)
auto merge(std::pmr::memory_resource * memory_resource_p, Tokens... result_tokens)
{
    return ChainJunction<typename Tokens::value_type...> {memory_resource_p, std::move(result_tokens)...};
}

LCF_ASYNC_EXPORT
template <details::chain::result_token_c... Tokens>
requires (sizeof...(Tokens) > 0u)
auto merge(Tokens... result_tokens)
{
    return merge(std::pmr::get_default_resource(), std::move(result_tokens)...);
}

}
