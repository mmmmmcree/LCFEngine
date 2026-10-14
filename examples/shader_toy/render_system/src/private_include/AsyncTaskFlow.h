#pragma once

#include <mutex>
#include <asio/any_io_executor.hpp>
#include <asio/awaitable.hpp>
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/experimental/concurrent_channel.hpp>
#include <asio/post.hpp>
#include <asio/strand.hpp>
#include <asio/this_coro.hpp>
#include <asio/use_awaitable.hpp>
#include <cassert>
#include <concepts>
#include <exception>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace lcf::shader_toy {

namespace details::task_flow {

template <typename T>
using TaskResult = std::expected<T, std::error_code>;

using CompletionChannel = asio::experimental::concurrent_channel<void(std::error_code)>;

inline std::error_code exception_error(std::exception_ptr exception) noexcept
{
    try { std::rethrow_exception(exception); }
    catch (const std::system_error & error) { return error.code(); }
    catch (...) { return std::make_error_code(std::errc::io_error); }
}

template <typename NodeType>
using node_value_t = typename std::remove_cvref_t<decltype(std::declval<const NodeType &>().result())>::value_type;

template <typename... NodeTypes>
struct MergedValues
{
    using NodeTuple = std::tuple<std::shared_ptr<NodeTypes>...>;

    NodeTuple m_nodes;
};

template <typename T>
struct result_arguments_traits
{
    using type = std::tuple<const T &>;
    static type get(const TaskResult<T> & result) { return std::tie(result.value()); }
};

template <>
struct result_arguments_traits<void>
{
    using type = std::tuple<>;
    static type get(const TaskResult<void> &) { return {}; }
};

template <typename... NodeTypes>
struct result_arguments_traits<MergedValues<NodeTypes...>>
{
    using type = decltype(std::tuple_cat(std::declval<typename result_arguments_traits<node_value_t<NodeTypes>>::type>()...));
    static type get(const TaskResult<MergedValues<NodeTypes...>> & result)
    {
        return std::apply([](const auto &... node_sp) {
            return std::tuple_cat(result_arguments_traits<node_value_t<NodeTypes>>::get(node_sp->result())...);
        }, result.value().m_nodes);
    }
};

template <typename T>
struct task_value_traits
{
    using type = T;
};

template <typename T>
struct task_value_traits<TaskResult<T>>
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

template <typename Function, typename Input>
using function_return_t = decltype(std::apply(std::declval<Function &>(), std::declval<typename result_arguments_traits<Input>::type>()));

template <typename T>
struct is_task_result_traits : std::false_type {};

template <typename T>
struct is_task_result_traits<TaskResult<T>> : std::true_type {};

template <typename T>
struct is_awaitable_traits : std::false_type {};

template <typename T, typename Executor>
struct is_awaitable_traits<asio::awaitable<T, Executor>> : std::true_type {};

template <typename Return>
TaskResult<task_value_t<Return>> as_task_result(Return result)
{
    using Value = task_value_t<Return>;
    if constexpr (std::same_as<Return, std::error_code>) {
        if (result) { return std::unexpected(result); }
        return {};
    } else if constexpr (is_task_result_traits<Return>::value) {
        return result;
    } else {
        return TaskResult<Value> {std::in_place, std::move(result)};
    }
}

template <typename Function, typename NodeType>
asio::awaitable<std::optional<TaskResult<task_value_t<function_return_t<Function, node_value_t<NodeType>>>>>>
invoke_task(Function function, std::shared_ptr<NodeType> input_sp)
{
    using Input = node_value_t<NodeType>;
    using Return = function_return_t<Function, Input>;
    co_await asio::post(asio::use_awaitable);
    auto arguments = result_arguments_traits<Input>::get(input_sp->result());
    if constexpr (is_awaitable_traits<std::remove_cvref_t<Return>>::value) {
        if constexpr (std::is_void_v<typename Return::value_type>) {
            co_await std::apply(function, arguments);
            co_return TaskResult<void> {};
        } else {
            co_return as_task_result(co_await std::apply(function, arguments));
        }
    } else if constexpr (std::is_void_v<Return>) {
        std::apply(function, arguments);
        co_return TaskResult<void> {};
    } else {
        co_return as_task_result(std::apply(function, arguments));
    }
}

template <typename NodeType>
asio::awaitable<void> run_node(std::shared_ptr<NodeType> node_sp)
{
    try {
        co_await asio::post(asio::use_awaitable);
        co_await node_sp->m_operation();
    } catch (...) {
        if (not node_sp->isReady()) { node_sp->publishError(exception_error(std::current_exception())); }
    }
    node_sp->m_operation = {};
}

template <typename NodeType>
void ensure_started(const std::shared_ptr<NodeType> & node_sp, const asio::any_io_executor & executor)
{
    if (not node_sp->tryStart()) { return; }
    try {
        asio::co_spawn(executor, run_node(node_sp), asio::detached);
    } catch (...) {
        node_sp->publishError(exception_error(std::current_exception()));
    }
}

template <typename NodeType>
asio::awaitable<void> wait_node(std::shared_ptr<NodeType> node_sp)
{
    if (node_sp->isReady()) { co_return; }
    auto executor = co_await asio::this_coro::executor;
    auto channel_sp = std::make_shared<CompletionChannel>(executor, 1u);
    if (not node_sp->subscribe(channel_sp)) { co_return; }
    co_await channel_sp->async_receive(asio::use_awaitable);
}

template <typename... NodeTypes>
asio::awaitable<std::optional<std::error_code>> wait_dependencies(std::shared_ptr<NodeTypes>... node_sps)
{
    auto executor = co_await asio::this_coro::executor;
    (ensure_started(node_sps, executor), ...);
    (co_await wait_node(node_sps), ...);
    std::optional<std::error_code> error_opt;
    ((error_opt = error_opt ? error_opt : node_sps->error()), ...);
    co_return error_opt;
}

template <typename NodeType>
asio::awaitable<void> wait_result(std::shared_ptr<NodeType> node_sp)
{
    auto executor = co_await asio::this_coro::executor;
    ensure_started(node_sp, executor);
    co_await wait_node(std::move(node_sp));
}

}

template <typename ReturnType>
class AsyncTaskFlow
{
    template <typename> friend class AsyncTaskFlow;
    using Result = details::task_flow::TaskResult<ReturnType>;
    class Node
    {
        using Self = Node;
        using CompletionChannel = details::task_flow::CompletionChannel;
        using WaiterList = std::vector<std::weak_ptr<CompletionChannel>>;
        using Operation = std::move_only_function<asio::awaitable<void>()>;
    public:
        ~Node() noexcept = default;
        Node() noexcept = default;
        Node(const Self &) = delete;
        Self & operator=(const Self &) = delete;
        Node(Self &&) = delete;
        Self & operator=(Self &&) = delete;
    public:
        bool tryStart() noexcept
        {
            std::scoped_lock lock {m_mutex};
            if (m_started or m_completed) { return false; }
            m_started = true;
            return true;
        }
        bool isReady() const noexcept
        {
            std::scoped_lock lock {m_mutex};
            return m_completed;
        }
        std::optional<std::error_code> error() const noexcept
        {
            std::scoped_lock lock {m_mutex};
            assert(m_completed);
            if (m_result_opt->has_value()) { return std::nullopt; }
            return m_result_opt->error();
        }
        bool subscribe(const std::shared_ptr<CompletionChannel> & channel_sp)
        {
            std::scoped_lock lock {m_mutex};
            if (m_completed) { return false; }
            m_waiters.emplace_back(channel_sp);
            return true;
        }
    public:
        void publish(Result result)
        {
            WaiterList waiters;
            {
                std::scoped_lock lock {m_mutex};
                assert(not m_completed);
                m_result_opt.emplace(std::move(result));
                m_completed = true;
                waiters = std::move(m_waiters);
            }
            for (const auto & channel_wp : waiters) {
                auto channel_sp = channel_wp.lock();
                if (not channel_sp) { continue; }
                channel_sp->try_send(std::error_code {});
            }
        }
        void publishError(std::error_code error)
        {
            this->publish(Result {std::unexpected(error)});
        }
        const Result & result() const noexcept
        {
            std::scoped_lock lock {m_mutex};
            assert(m_completed);
            return *m_result_opt;
        }
    public:
        Operation m_operation;
    private:
        mutable std::mutex m_mutex;
        WaiterList m_waiters;
        std::optional<Result> m_result_opt;
        bool m_completed = false;
        bool m_started = false;
    };
    using Self = AsyncTaskFlow;
    using NodeSharedPtr = std::shared_ptr<Node>;
public:
    ~AsyncTaskFlow() noexcept = default;
    template <typename Value = ReturnType>
        requires (std::same_as<Value, ReturnType> and not std::is_void_v<Value>)
    AsyncTaskFlow(Value initial_value, asio::any_io_executor executor) :
        m_node_sp(std::make_shared<Node>()), m_executor(std::move(executor))
    {
        m_node_sp->publish(Result {std::in_place, std::move(initial_value)});
    }
    explicit AsyncTaskFlow(asio::any_io_executor executor) requires std::is_void_v<ReturnType> :
        m_node_sp(std::make_shared<Node>()), m_executor(std::move(executor))
    {
        m_node_sp->publish(Result {});
    }
    AsyncTaskFlow(const Self &) noexcept = default;
    Self & operator=(const Self &) noexcept = default;
    AsyncTaskFlow(Self &&) noexcept = default;
    Self & operator=(Self &&) noexcept = default;
public:
    template <typename Function>
    auto link(Function function) const
    {
        return this->link(m_executor, std::move(function));
    }
    template <typename Function>
    auto link(asio::any_io_executor executor, Function function) const
    {
        this->requireValid();
        using Value = details::task_flow::task_value_t<details::task_flow::function_return_t<Function, ReturnType>>;
        using OutputFlow = AsyncTaskFlow<Value>;
        auto node_sp = std::make_shared<typename OutputFlow::Node>();
        auto * output_node_p = node_sp.get();
        node_sp->m_operation = [input_sp = m_node_sp, output_node_p, executor, function = std::move(function)]() mutable -> asio::awaitable<void> {
            auto error_opt = co_await details::task_flow::wait_dependencies(input_sp);
            if (error_opt) {
                output_node_p->publishError(*error_opt);
                co_return;
            }
            auto result_opt = co_await asio::co_spawn(executor,
                details::task_flow::invoke_task(std::move(function), input_sp), asio::use_awaitable);
            output_node_p->publish(std::move(result_opt).value());
        };
        return OutputFlow {std::move(node_sp), std::move(executor)};
    }
    template <typename... OtherTypes> requires (sizeof...(OtherTypes) != 0u)
    auto merge(const AsyncTaskFlow<OtherTypes> &... others) const
    {
        this->requireValid();
        (others.requireValid(), ...);
        using Value = details::task_flow::MergedValues<Node, typename AsyncTaskFlow<OtherTypes>::Node...>;
        using OutputFlow = AsyncTaskFlow<Value>;
        auto node_sp = std::make_shared<typename OutputFlow::Node>();
        auto * output_node_p = node_sp.get();
        typename Value::NodeTuple inputs {m_node_sp, others.m_node_sp...};
        node_sp->m_operation = [inputs = std::move(inputs), output_node_p]() -> asio::awaitable<void> {
            auto error_opt = co_await std::apply([](const auto &... input_sp) {
                return details::task_flow::wait_dependencies(input_sp...);
            }, inputs);
            if (error_opt) {
                output_node_p->publishError(*error_opt);
                co_return;
            }
            output_node_p->publish(details::task_flow::TaskResult<Value> {std::in_place, Value {inputs}});
        };
        return OutputFlow {std::move(node_sp), m_executor};
    }
    template <typename Completion>
    void start(asio::any_io_executor executor, Completion completion) &&
    {
        this->requireValid();
        auto node_sp = std::move(m_node_sp);
        auto result_node_sp = node_sp;
        asio::co_spawn(asio::make_strand(executor), details::task_flow::wait_result(std::move(node_sp)),
            [result_node_sp = std::move(result_node_sp), completion = std::move(completion)](std::exception_ptr exception) mutable {
                if (exception) {
                    Result result {std::unexpected(details::task_flow::exception_error(exception))};
                    std::invoke(completion, result);
                    return;
                }
                std::invoke(completion, result_node_sp->result());
            });
    }
    template <typename Completion>
    void start(Completion completion) &&
    {
        auto executor = m_executor;
        std::move(*this).start(std::move(executor), std::move(completion));
    }
    bool isReady() const noexcept { return m_node_sp and m_node_sp->isReady(); }
    const Result & getResult() const
    {
        this->requireValid();
        if (not m_node_sp->isReady()) { throw std::logic_error("an async task flow result is not ready"); }
        return m_node_sp->result();
    }
private:
    AsyncTaskFlow(NodeSharedPtr node_sp, asio::any_io_executor executor) noexcept :
        m_node_sp(std::move(node_sp)), m_executor(std::move(executor)) {}
private:
    void requireValid() const
    {
        if (not m_node_sp) { throw std::logic_error("an async task flow handle has been consumed"); }
    }
private:
    NodeSharedPtr m_node_sp;
    asio::any_io_executor m_executor;
};

template <typename T>
AsyncTaskFlow(T, asio::any_io_executor) -> AsyncTaskFlow<T>;

AsyncTaskFlow(asio::any_io_executor) -> AsyncTaskFlow<void>;

}
