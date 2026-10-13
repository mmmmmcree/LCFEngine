#pragma once

#include <asio/any_io_executor.hpp>
#include <asio/awaitable.hpp>
#include <asio/co_spawn.hpp>
#include <asio/use_awaitable.hpp>
#include <atomic>
#include <cassert>
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
#include <variant>

namespace lcf::shader_toy {

template <typename T> class AsyncTaskChain;

/*
 * TODO(async-task-chain):
 * - Constrain function and dependency templates with concepts.
 * - Measure shared-node allocation fragmentation and evaluate pooled or co-allocated storage for cross-thread result handles.
 * - Add coverage for one-shot consumption, predecessor ordering, failure short-circuiting, and variadic dependencies.
 */

template <typename T>
class AsyncChainTaskFuture
{
    template <typename> friend class AsyncTaskChain;
    using Self = AsyncChainTaskFuture;
public:
    ~AsyncChainTaskFuture() noexcept = default;
    AsyncChainTaskFuture() noexcept = delete;
    AsyncChainTaskFuture(const Self &) noexcept = default;
    Self & operator=(const Self &) noexcept = default;
    AsyncChainTaskFuture(Self &&) noexcept = default;
    Self & operator=(Self &&) noexcept = default;
private:
    explicit AsyncChainTaskFuture(std::shared_ptr<void> node_sp) noexcept : m_node_sp(std::move(node_sp)) {}
private:
    std::shared_ptr<void> m_node_sp;
};

namespace details {

std::error_code exception_error(std::exception_ptr exception) noexcept;

template <typename T>
using TaskResult = std::expected<T, std::error_code>;

enum class TaskState : std::uint8_t
{
    ePending,
    eRunning,
    eCompleted
};

template <typename T>
struct task_value_traits
{
    using type = T;
};

template <typename T>
struct task_value_traits<std::expected<T, std::error_code>> : task_value_traits<T> {};

template <typename T, typename Executor>
struct task_value_traits<asio::awaitable<T, Executor>> : task_value_traits<T> {};

template <>
struct task_value_traits<void>
{
    using type = std::monostate;
};

template <>
struct task_value_traits<std::error_code> : task_value_traits<void> {};

template <typename T>
using task_value_t = typename task_value_traits<T>::type;

template <typename T>
using task_result_t = TaskResult<task_value_t<T>>;

template <typename T>
struct is_awaitable_traits : std::false_type {};

template <typename T, typename Executor>
struct is_awaitable_traits<asio::awaitable<T, Executor>> : std::true_type {};

template <typename Value, typename Return>
TaskResult<Value> as_task_result(Return result)
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

template <typename Function, typename... Values>
asio::awaitable<task_result_t<std::invoke_result_t<Function &, const Values &...>>>
invoke_task(Function function, const Values &... values)
{
    using Return = std::invoke_result_t<Function &, const Values &...>;
    using Value = task_value_t<Return>;
    if constexpr (is_awaitable_traits<Return>::value) {
        if constexpr (std::is_void_v<typename Return::value_type>) {
            co_await std::invoke(function, values...);
            co_return TaskResult<Value> {Value {}};
        } else {
            co_return as_task_result<Value>(co_await std::invoke(function, values...));
        }
    } else if constexpr (std::is_void_v<Return>) {
        std::invoke(function, values...);
        co_return TaskResult<Value> {Value {}};
    } else {
        co_return as_task_result<Value>(std::invoke(function, values...));
    }
}

}

template <typename ReturnType>
class AsyncTaskChain
{
    template <typename> friend class AsyncTaskChain;
    using Self = AsyncTaskChain;
    struct Node
    {
        using Result = details::TaskResult<ReturnType>;
        using Operation = std::move_only_function<asio::awaitable<void>()>;

        void publish(Result result) noexcept
        {
            if (m_state.load(std::memory_order_relaxed) == details::TaskState::eCompleted) {
                assert(false && "an async task node can only publish once");
                return;
            }
            m_result_opt.emplace(std::move(result));
            m_state.store(details::TaskState::eCompleted, std::memory_order_release);
        }
        bool tryStart() noexcept
        {
            auto expected = details::TaskState::ePending;
            return m_state.compare_exchange_strong(
                expected, details::TaskState::eRunning, std::memory_order_acq_rel, std::memory_order_acquire);
        }
        bool isReady() const noexcept
        {
            return m_state.load(std::memory_order_acquire) == details::TaskState::eCompleted;
        }
        const Result & result() const noexcept
        {
            return *m_result_opt;
        }

        std::optional<Result> m_result_opt;
        Operation m_operation;
    private:
        std::atomic<details::TaskState> m_state = details::TaskState::ePending;
    };
    using NodeSharedPtr = std::shared_ptr<Node>;
    using Result = typename Node::Result;
    using ResultHandle = AsyncChainTaskFuture<ReturnType>;

    static asio::awaitable<void> run_node(NodeSharedPtr node)
    {
        try {
            if (node->isReady()) { co_return; }
            const bool started = node->tryStart();
            assert(started && "an async task node must only be started once");
            if (not started) { co_return; }
            co_await node->m_operation();
        } catch (...) {
            if (not node->isReady()) {
                node->publish(std::unexpected(details::exception_error(std::current_exception())));
            }
        }
    }

public:
    ~AsyncTaskChain() noexcept = default;
    AsyncTaskChain(ReturnType initial_value, asio::any_io_executor executor) :
        m_node_sp(std::make_shared<Node>()), m_executor(std::move(executor))
    {
        m_node_sp->publish(Result {std::in_place, std::move(initial_value)});
    }
    AsyncTaskChain(const Self &) = delete;
    Self & operator=(const Self &) = delete;
    AsyncTaskChain(Self &&) noexcept = default;
    Self & operator=(Self &&) noexcept = default;
public:
    template <typename Function, typename... DependencyTypes>
    auto link(Function function, AsyncChainTaskFuture<DependencyTypes>... dependencies) &&
    {
        return std::move(*this).link(m_executor, std::move(function), std::move(dependencies)...);
    }

    template <typename Function, typename... DependencyTypes>
    auto link(asio::any_io_executor executor, Function function,
        AsyncChainTaskFuture<DependencyTypes>... dependencies) &&
    {
        using LinkedReturnType = typename details::task_value_traits<std::invoke_result_t<
            Function &, const ReturnType &, const DependencyTypes &...>>::type;
        auto node_sp = std::make_shared<typename AsyncTaskChain<LinkedReturnType>::Node>();
        auto input_sp = std::move(m_node_sp);
        auto dependency_sps = std::tuple<std::shared_ptr<typename AsyncTaskChain<DependencyTypes>::Node>...> {
            std::static_pointer_cast<typename AsyncTaskChain<DependencyTypes>::Node>(std::move(dependencies).m_node_sp)...
        };
        auto * output_node_p = node_sp.get();
        node_sp->m_operation = [input_sp = std::move(input_sp), dependency_sps = std::move(dependency_sps),
                                    output_node_p, executor, function = std::move(function)]() mutable -> asio::awaitable<void> {
            co_await AsyncTaskChain::run_node(input_sp);
            const auto & input_result = input_sp->result();
            if (not input_result) {
                output_node_p->publish(std::unexpected(input_result.error()));
                co_return;
            }
            // A dependency is an earlier node in this chain and is complete with input_sp.
            auto result = co_await std::apply(
                [&](const auto &... dependency_sp) {
                    return asio::co_spawn(executor, details::invoke_task(
                        std::move(function), input_result.value(), dependency_sp->result().value()...), asio::use_awaitable);
                }, dependency_sps);
            output_node_p->publish(std::move(result));
        };
        return AsyncTaskChain<LinkedReturnType> {std::move(node_sp), std::move(executor)};
    }
    template <typename Completion>
    void start(asio::any_io_executor executor, Completion completion) &&
    {
        auto node_sp = std::move(m_node_sp);
        auto result_node_sp = node_sp;
        asio::co_spawn(executor, AsyncTaskChain::run_node(std::move(node_sp)),
            [result_node_sp = std::move(result_node_sp), completion = std::move(completion)](std::exception_ptr exception) mutable {
                if (exception) { std::rethrow_exception(exception); }
                std::invoke(completion, result_node_sp->result());
            });
    }
    template <typename Completion>
    void start(Completion completion) &&
    {
        auto executor = std::move(m_executor);
        std::move(*this).start(std::move(executor), std::move(completion));
    }
    auto getResult() const noexcept { return ResultHandle {m_node_sp}; }
private:
    AsyncTaskChain(NodeSharedPtr node_sp, asio::any_io_executor executor) noexcept :
        m_node_sp(std::move(node_sp)), m_executor(std::move(executor)) {}
private:
    NodeSharedPtr m_node_sp;
    asio::any_io_executor m_executor;
};

}
