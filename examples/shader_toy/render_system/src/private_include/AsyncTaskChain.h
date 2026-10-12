#pragma once

#include <asio/any_io_executor.hpp>
#include <asio/awaitable.hpp>
#include <asio/co_spawn.hpp>
#include <asio/steady_timer.hpp>
#include <asio/this_coro.hpp>
#include <asio/use_awaitable.hpp>
#include <atomic>
#include <cassert>
#include <chrono>
#include <concepts>
#include <exception>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <system_error>
#include <type_traits>
#include <utility>
#include <variant>

namespace lcf::shader_toy {

template <typename T> class AsyncTaskChain;

namespace task_details {

std::error_code exception_error(std::exception_ptr exception) noexcept;

template <typename T>
struct TaskValue { using Type = T; };

template <typename T>
struct TaskValue<std::expected<T, std::error_code>> : TaskValue<T> {};

template <typename T, typename Executor>
struct TaskValue<asio::awaitable<T, Executor>> : TaskValue<T> {};

template <>
struct TaskValue<void> { using Type = std::monostate; };

template <>
struct TaskValue<std::error_code> : TaskValue<void> {};

template <typename T>
struct IsAwaitable : std::false_type {};

template <typename T, typename Executor>
struct IsAwaitable<asio::awaitable<T, Executor>> : std::true_type {};

template <typename T>
struct TaskState
{
    using Result = std::expected<T, std::error_code>;
    using ResultOptional = std::optional<Result>;

    ResultOptional m_result_opt;
    std::atomic_bool m_ready = false;
};

template <typename T>
struct TaskNode
{
    using StatePointer = std::shared_ptr<TaskState<T>>;
    using Operation = std::move_only_function<asio::awaitable<void>()>;

    StatePointer m_state = std::make_shared<TaskState<T>>();
    Operation m_operation;
    std::atomic_bool m_started = false;
};

template <typename T>
void publish_result(const std::shared_ptr<TaskState<T>> & state, typename TaskState<T>::Result result)
{
    state->m_result_opt.emplace(std::move(result));
    state->m_ready.store(true, std::memory_order_release);
}

template <typename T>
asio::awaitable<void> run_node(std::shared_ptr<TaskNode<T>> node)
{
    if (node->m_state->m_ready.load(std::memory_order_acquire)) { co_return; }
    if (node->m_started.exchange(true, std::memory_order_acq_rel)) {
        asio::steady_timer retry_timer {co_await asio::this_coro::executor};
        while (not node->m_state->m_ready.load(std::memory_order_acquire)) {
            retry_timer.expires_after(std::chrono::milliseconds {1});
            co_await retry_timer.async_wait(asio::use_awaitable);
        }
        co_return;
    }
    try { co_await node->m_operation(); }
    catch (...) { publish_result<T>(node->m_state, std::unexpected(exception_error(std::current_exception()))); }
}

template <typename Value, typename Return>
std::expected<Value, std::error_code> task_result(Return result)
{
    if constexpr (std::same_as<Return, std::error_code>) {
        if (result) { return std::unexpected(result); }
        return Value {};
    } else if constexpr (std::same_as<Return, std::expected<void, std::error_code>>) {
        if (not result) { return std::unexpected(result.error()); }
        return Value {};
    } else {
        return std::move(result);
    }
}

template <typename T, typename Function>
asio::awaitable<std::optional<std::expected<typename TaskValue<std::invoke_result_t<Function &, const T &>>::Type, std::error_code>>>
invoke_task(Function function, std::shared_ptr<TaskState<T>> input)
{
    using Return = std::invoke_result_t<Function &, const T &>;
    using Value = typename TaskValue<Return>::Type;
    const auto & value = input->m_result_opt->value();
    if constexpr (IsAwaitable<Return>::value) {
        if constexpr (std::is_void_v<typename Return::value_type>) {
            co_await std::invoke(function, value);
            co_return std::expected<Value, std::error_code> {Value {}};
        } else {
            co_return task_result<Value>(co_await std::invoke(function, value));
        }
    } else if constexpr (std::is_void_v<Return>) {
        std::invoke(function, value);
        co_return std::expected<Value, std::error_code> {Value {}};
    } else {
        co_return task_result<Value>(std::invoke(function, value));
    }
}

}

template <typename T>
class AsyncTaskResult
{
    template <typename> friend class AsyncTaskChain;
    using Self = AsyncTaskResult;
    using StatePointer = std::shared_ptr<task_details::TaskState<T>>;
public:
    using Result = std::expected<T, std::error_code>;
public:
    ~AsyncTaskResult() noexcept = default;
    AsyncTaskResult() noexcept = default;
    AsyncTaskResult(const Self &) noexcept = default;
    Self & operator=(const Self &) noexcept = default;
    AsyncTaskResult(Self &&) noexcept = default;
    Self & operator=(Self &&) noexcept = default;
public:
    bool isReady() const noexcept { return m_state and m_state->m_ready.load(std::memory_order_acquire); }
    const Result & getResult() const noexcept
    {
        assert(this->isReady());
        return *m_state->m_result_opt;
    }
private:
    explicit AsyncTaskResult(StatePointer state) noexcept : m_state(std::move(state)) {}
private:
    StatePointer m_state;
};

template <typename T>
class AsyncTaskChain
{
    template <typename> friend class AsyncTaskChain;
    using Self = AsyncTaskChain;
    using NodePointer = std::shared_ptr<task_details::TaskNode<T>>;
public:
    ~AsyncTaskChain() noexcept = default;
    AsyncTaskChain(T value, asio::any_io_executor executor) :
        m_node(std::make_shared<task_details::TaskNode<T>>()), m_executor(std::move(executor))
    {
        task_details::publish_result<T>(m_node->m_state, std::expected<T, std::error_code> {std::in_place, std::move(value)});
    }
    AsyncTaskChain(const Self &) noexcept = default;
    Self & operator=(const Self &) noexcept = default;
    AsyncTaskChain(Self &&) noexcept = default;
    Self & operator=(Self &&) noexcept = default;
public:
    template <typename Function>
    auto link(Function function) const { return this->link(m_executor, std::move(function)); }

    template <typename Function>
    auto link(asio::any_io_executor executor, Function function) const
    {
        using Value = typename task_details::TaskValue<std::invoke_result_t<Function &, const T &>>::Type;
        auto node = std::make_shared<task_details::TaskNode<Value>>();
        node->m_operation = [input = m_node, output = node->m_state, executor, function = std::move(function)]() mutable -> asio::awaitable<void> {
            co_await task_details::run_node(input);
            const auto & input_result = *input->m_state->m_result_opt;
            if (not input_result) {
                task_details::publish_result<Value>(output, std::unexpected(input_result.error()));
                co_return;
            }
            auto result_opt = co_await asio::co_spawn(executor,
                task_details::invoke_task(std::move(function), input->m_state), asio::use_awaitable);
            task_details::publish_result<Value>(output, std::move(*result_opt));
        };
        return AsyncTaskChain<Value> {std::move(node), std::move(executor)};
    }

    AsyncTaskResult<T> getResult() const noexcept { return AsyncTaskResult<T> {m_node->m_state}; }

    template <typename Completion>
    void start(asio::any_io_executor executor, Completion completion) const
    {
        asio::co_spawn(executor, task_details::run_node(m_node),
            [result = this->getResult(), completion = std::move(completion)](std::exception_ptr exception) mutable {
                if (exception) { std::rethrow_exception(exception); }
                std::invoke(completion, result);
            });
    }
private:
    AsyncTaskChain(NodePointer node, asio::any_io_executor executor) noexcept :
        m_node(std::move(node)), m_executor(std::move(executor)) {}
private:
    NodePointer m_node;
    asio::any_io_executor m_executor;
};

}
