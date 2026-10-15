#pragma once

#include "Chain.h"
#include <asio/detached.hpp>
#include <asio/experimental/concurrent_channel.hpp>
#include <asio/this_coro.hpp>
#include <cassert>
#include <variant>
#include <vector>

namespace lcf::async::task {

namespace details {

using CompletionChannel = asio::experimental::concurrent_channel<void(std::error_code)>;

template <typename NodeType>
asio::awaitable<void> run_flow_node(std::shared_ptr<NodeType> node_sp)
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
        asio::co_spawn(executor, run_flow_node(node_sp), asio::detached);
    } catch (...) {
        node_sp->publishError(exception_error(std::current_exception()));
        node_sp->m_operation = {};
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

}

template <details::value_c T>
class Flow
{
    friend struct details::FlowAccess;
    using Self = Flow;
    class Node
    {
        using StoredResult = std::variant<Result<T>, ResultView<T>>;
        using WaiterList = std::vector<std::weak_ptr<details::CompletionChannel>>;
        using Operation = std::move_only_function<asio::awaitable<void>()>;
    public:
        bool tryStart()
        {
            std::lock_guard lock {m_mutex};
            if (m_state != State::ePending) { return false; }
            m_state = State::eRunning;
            return true;
        }
        bool isReady() const
        {
            std::lock_guard lock {m_mutex};
            return m_state == State::eCompleted;
        }
        bool subscribe(const std::shared_ptr<details::CompletionChannel> & channel_sp)
        {
            std::lock_guard lock {m_mutex};
            if (m_state == State::eCompleted) { return false; }
            m_waiters.emplace_back(channel_sp);
            return true;
        }
        void publish(Result<T> result)
        {
            this->publishStored(StoredResult {std::in_place_index<0>, std::move(result)});
        }
        void publish(ResultView<T> result_sp)
        {
            if (not result_sp) { throw std::logic_error("a flow result view must own a result"); }
            this->publishStored(StoredResult {std::in_place_index<1>, std::move(result_sp)});
        }
        void publishError(std::error_code error) { this->publish(Result<T> {std::unexpected(error)}); }
        const Result<T> & result() const
        {
            std::lock_guard lock {m_mutex};
            assert(m_state == State::eCompleted);
            return std::visit([](const auto & stored) -> const Result<T> & {
                if constexpr (std::same_as<std::remove_cvref_t<decltype(stored)>, Result<T>>) { return stored; }
                else { return *stored; }
            }, *m_result_opt);
        }
    public:
        Operation m_operation;
    private:
        void publishStored(StoredResult result)
        {
            WaiterList waiters;
            {
                std::lock_guard lock {m_mutex};
                assert(m_state != State::eCompleted);
                m_result_opt.emplace(std::move(result));
                m_state = State::eCompleted;
                waiters = std::move(m_waiters);
            }
            for (const auto & waiter_wp : waiters) {
                if (auto channel_sp = waiter_wp.lock()) { channel_sp->try_send(std::error_code {}); }
            }
        }
    private:
        mutable std::mutex m_mutex;
        WaiterList m_waiters;
        std::optional<StoredResult> m_result_opt;
        State m_state = State::ePending;
    };
    using NodeSharedPtr = std::shared_ptr<Node>;
public:
    using value_type = T;
public:
    ~Flow() noexcept = default;
    template <typename Value>
        requires (not std::is_void_v<T> and std::same_as<std::remove_cvref_t<Value>, T>
            and std::constructible_from<T, Value>)
    Flow(Value && initial_value, asio::any_io_executor executor) :
        m_node_sp(std::make_shared<Node>()), m_executor(std::move(executor))
    {
        m_node_sp->publish(Result<T> {std::in_place, std::forward<Value>(initial_value)});
    }
    explicit Flow(asio::any_io_executor executor) requires std::is_void_v<T> :
        m_node_sp(std::make_shared<Node>()), m_executor(std::move(executor))
    {
        m_node_sp->publish(Result<void> {});
    }
    Flow(const Self &) noexcept = default;
    Flow(Self &&) noexcept = default;
    Self & operator=(const Self &) noexcept = default;
    Self & operator=(Self &&) noexcept = default;
public:
    template <typename Function>
        requires details::task_function_c<Function, T>
    auto link(Function function) const { return this->link(m_executor, std::move(function)); }
    template <typename Function>
        requires details::task_function_c<Function, T>
    auto link(asio::any_io_executor executor, Function function) const
    {
        this->requireValid();
        using Value = details::task_value_t<details::function_return_t<Function, T>>;
        return Chain<Value>::from_operation(executor,
            [input = *this, executor, function = std::move(function)]() mutable -> asio::awaitable<std::optional<Result<Value>>> {
                auto result_sp = co_await input.run();
                if (not *result_sp) { co_return Result<Value> {std::unexpected(result_sp->error())}; }
                auto arguments = details::result_arguments_traits<T>::get(*result_sp);
                co_return co_await asio::co_spawn(executor,
                    details::invoke_task(std::move(function), std::move(arguments)), asio::use_awaitable);
            });
    }
    asio::awaitable<ResultView<T>> run() const
    {
        this->requireValid();
        return run_owned(m_node_sp);
    }
    template <typename Completion>
        requires details::completion_c<Completion, T>
    void start(asio::any_io_executor executor, Completion completion) &&
    {
        this->requireValid();
        asio::co_spawn(executor, run_owned(std::move(m_node_sp)),
            [completion = std::move(completion)](std::exception_ptr exception, ResultView<T> result_sp) mutable {
                if (exception) {
                    const Result<T> result {std::unexpected(details::exception_error(exception))};
                    std::invoke(completion, result);
                    return;
                }
                std::invoke(completion, *result_sp);
            });
    }
    template <typename Completion>
        requires details::completion_c<Completion, T>
    void start(Completion completion) && { std::move(*this).start(m_executor, std::move(completion)); }
    bool isReady() const
    {
        this->requireValid();
        return m_node_sp->isReady();
    }
    const Result<T> & getResult() const
    {
        this->requireValid();
        if (not m_node_sp->isReady()) { throw std::logic_error("a flow result is not ready"); }
        return m_node_sp->result();
    }
    asio::any_io_executor getExecutor() const
    {
        this->requireValid();
        return m_executor;
    }
private:
    Flow(NodeSharedPtr node_sp, asio::any_io_executor executor) noexcept :
        m_node_sp(std::move(node_sp)), m_executor(std::move(executor)) {}
    void requireValid() const
    {
        if (not m_node_sp) { throw std::logic_error("an async flow handle has been consumed"); }
    }
    template <typename Operation>
    static Self from_operation(asio::any_io_executor executor, Operation operation)
    {
        auto node_sp = std::make_shared<Node>();
        auto * output_node_p = node_sp.get();
        node_sp->m_operation = [output_node_p, operation = std::move(operation)]() mutable -> asio::awaitable<void> {
            output_node_p->publish(co_await operation());
        };
        return Self {std::move(node_sp), std::move(executor)};
    }
    static asio::awaitable<ResultView<T>> run_owned(NodeSharedPtr node_sp)
    {
        auto executor = co_await asio::this_coro::executor;
        details::ensure_started(node_sp, executor);
        co_await details::wait_node(node_sp);
        const auto * result_p = &node_sp->result();
        co_return ResultView<T> {std::move(node_sp), result_p};
    }
private:
    NodeSharedPtr m_node_sp;
    asio::any_io_executor m_executor;
};

template <typename T>
Flow(T, asio::any_io_executor) -> Flow<T>;

Flow(asio::any_io_executor) -> Flow<void>;

namespace details {

struct FlowAccess
{
    template <typename T, typename Operation>
    static Flow<T> make(asio::any_io_executor executor, Operation operation)
    {
        return Flow<T>::from_operation(std::move(executor), std::move(operation));
    }
    template <typename T>
    static void start(const Flow<T> & flow, const asio::any_io_executor & executor)
    {
        flow.requireValid();
        ensure_started(flow.m_node_sp, executor);
    }
};

}

}
