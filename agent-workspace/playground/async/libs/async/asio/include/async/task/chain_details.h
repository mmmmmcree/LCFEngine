#pragma once

#include "common.h"
#include <atomic>

namespace lcf::async::task::details::chain {

template <result_value_c T>
struct ExecutionState
{
    using Self = ExecutionState;
    using Operation = std::move_only_function<asio::awaitable<void>(Self &)>;
public:
    ExecutionState() noexcept = default;
    explicit ExecutionState(Operation operation) noexcept : m_operation(std::move(operation)) {}
public:
    void publish(Result<T> result) noexcept(std::is_nothrow_move_constructible_v<Result<T>>)
    {
        m_result_placeholder.emplace(std::move(result));
        m_ready.store(true, std::memory_order_release);
    }
    void publish(std::unexpected<std::error_code> failure) noexcept
    {
        m_result_placeholder.emplace(std::unexpect, failure.error());
        m_ready.store(true, std::memory_order_release);
    }
    bool isReady() const noexcept { return m_ready.load(std::memory_order_acquire); }
    const Result<T> & result() const noexcept { return *m_result_placeholder; }
public:
    Operation m_operation;
private:
    std::optional<Result<T>> m_result_placeholder;
    std::atomic_bool m_ready = false;
};

template <result_value_c T>
using ExecutionStateHandle = std::shared_ptr<ExecutionState<T>>;

template <result_value_c T>
class ResultToken
{
    template <result_value_c> friend class lcf::async::task::Chain;
    template <result_value_c...> friend class lcf::async::task::ChainJunction;
    using Self = ResultToken;
    using ExecutionStateHandle = chain::ExecutionStateHandle<T>;
public:
    using value_type = T;
public:
    ~ResultToken() noexcept = default;
    ResultToken() = delete;
    ResultToken(const Self &) noexcept = default;
    ResultToken(Self &&) noexcept = default;
    Self & operator=(const Self &) noexcept = default;
    Self & operator=(Self &&) noexcept = default;
private:
    explicit ResultToken(ExecutionStateHandle execution_state_handle) noexcept : m_execution_state_handle(std::move(execution_state_handle)) {}
    bool isReady() const noexcept { return m_execution_state_handle->isReady(); }
    const Result<T> & result() const noexcept { return m_execution_state_handle->result(); }
private:
    ExecutionStateHandle m_execution_state_handle;
};

template <typename Token>
concept result_token_c = requires {
    typename std::remove_cvref_t<Token>::value_type;
    requires std::same_as<std::remove_cvref_t<Token>, ResultToken<typename std::remove_cvref_t<Token>::value_type>>;
};

}
