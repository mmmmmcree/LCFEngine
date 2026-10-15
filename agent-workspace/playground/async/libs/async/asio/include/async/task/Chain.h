#pragma once

#include "common.h"
#include <asio/co_spawn.hpp>
#include <atomic>
#include <cassert>
#include <stdexcept>

namespace lcf::async::task {

namespace details {

template <typename Handle>
concept chain_result_handle_c = requires {
    typename std::remove_cvref_t<Handle>::value_type;
    typename Chain<typename std::remove_cvref_t<Handle>::value_type>::ResultHandle;
} and std::same_as<std::remove_cvref_t<Handle>,
    typename Chain<typename std::remove_cvref_t<Handle>::value_type>::ResultHandle>;

}

template <details::value_c T>
class Chain
{
    template <details::value_c> friend class Chain;
    template <details::value_c> friend class Flow;
    using Self = Chain;
    class Node
    {
        using Operation = std::move_only_function<asio::awaitable<std::optional<Result<T>>>()>;
    public:
        void publish(Result<T> result)
        {
            assert(not this->isReady());
            m_result_opt.emplace(std::move(result));
            m_state.store(State::eCompleted, std::memory_order_release);
        }
        bool tryStart() noexcept
        {
            auto expected = State::ePending;
            return m_state.compare_exchange_strong(
                expected, State::eRunning, std::memory_order_acq_rel, std::memory_order_acquire);
        }
        bool isReady() const noexcept { return m_state.load(std::memory_order_acquire) == State::eCompleted; }
        const Result<T> & result() const noexcept
        {
            assert(this->isReady());
            return *m_result_opt;
        }
    public:
        Operation m_operation;
    private:
        std::optional<Result<T>> m_result_opt;
        std::atomic<State> m_state = State::ePending;
    };
    using NodeSharedPtr = std::shared_ptr<Node>;
public:
    using value_type = T;
    class ResultHandle
    {
        template <details::value_c> friend class Chain;
        using Self = ResultHandle;
    public:
        using value_type = T;
    public:
        ~ResultHandle() noexcept = default;
        ResultHandle() = delete;
        ResultHandle(const Self &) noexcept = default;
        ResultHandle(Self &&) noexcept = default;
        Self & operator=(const Self &) noexcept = default;
        Self & operator=(Self &&) noexcept = default;
    public:
        bool isReady() const noexcept { return m_node_sp and m_node_sp->isReady(); }
        const Result<T> & getResult() const
        {
            if (not this->isReady()) { throw std::logic_error("a chain result is not ready"); }
            return m_node_sp->result();
        }
    private:
        explicit ResultHandle(NodeSharedPtr node_sp) noexcept : m_node_sp(std::move(node_sp)) {}
    private:
        NodeSharedPtr m_node_sp;
    };
public:
    ~Chain() noexcept = default;
    template <typename Value>
        requires (not std::is_void_v<T> and std::same_as<std::remove_cvref_t<Value>, T>
            and std::constructible_from<T, Value>)
    Chain(Value && initial_value, asio::any_io_executor executor) :
        m_node_sp(std::make_shared<Node>()), m_executor(std::move(executor))
    {
        m_node_sp->publish(Result<T> {std::in_place, std::forward<Value>(initial_value)});
    }
    explicit Chain(asio::any_io_executor executor) requires std::is_void_v<T> :
        m_node_sp(std::make_shared<Node>()), m_executor(std::move(executor))
    {
        m_node_sp->publish(Result<void> {});
    }
    Chain(const Self &) = delete;
    Chain(Self &&) noexcept = default;
    Self & operator=(const Self &) = delete;
    Self & operator=(Self &&) noexcept = default;
public:
    template <typename Function, details::chain_result_handle_c... Handles>
        requires details::task_function_c<Function, T, typename Handles::value_type...>
    auto link(Function function, Handles... dependencies) &&
    {
        return std::move(*this).link(m_executor, std::move(function), std::move(dependencies)...);
    }
    template <typename Function, details::chain_result_handle_c... Handles>
        requires details::task_function_c<Function, T, typename Handles::value_type...>
    auto link(asio::any_io_executor executor, Function function, Handles... dependencies) &&
    {
        this->requireValid();
        if ((... or not dependencies.m_node_sp)) { throw std::logic_error("a chain dependency has been consumed"); }
        using Value = details::task_value_t<details::function_return_t<Function, T, typename Handles::value_type...>>;
        auto input_sp = std::move(m_node_sp);
        auto dependency_sps = std::tuple {std::move(dependencies.m_node_sp)...};
        return Chain<Value>::from_operation(executor,
            [input_sp = std::move(input_sp), dependency_sps = std::move(dependency_sps),
                executor, function = std::move(function)]() mutable -> asio::awaitable<std::optional<Result<Value>>> {
                co_await Self::run_node(input_sp);
                const auto & input_result = input_sp->result();
                if (not input_result) { co_return Result<Value> {std::unexpected(input_result.error())}; }
                // Handles refer to earlier nodes in this chain; they do not start independent work.
                const bool dependencies_ready = std::apply([](const auto &... dependency_sp) {
                    return (... and dependency_sp->isReady());
                }, dependency_sps);
                if (not dependencies_ready) { throw std::logic_error("a chain dependency is not an earlier completed node"); }
                auto arguments = std::apply([&](const auto &... dependency_sp) {
                    return std::tuple_cat(details::result_arguments(input_result),
                        details::result_arguments(dependency_sp->result())...);
                }, dependency_sps);
                co_return co_await asio::co_spawn(executor,
                    details::invoke_task(std::move(function), std::move(arguments)), asio::use_awaitable);
            });
    }
    asio::awaitable<ResultView<T>> run() &&
    {
        this->requireValid();
        return run_owned(std::move(m_node_sp));
    }
    template <typename Completion>
        requires details::completion_c<Completion, T>
    void start(asio::any_io_executor executor, Completion completion) &&
    {
        auto operation = std::move(*this).run();
        asio::co_spawn(executor, std::move(operation),
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
    ResultHandle getResult() const
    {
        this->requireValid();
        return ResultHandle {m_node_sp};
    }
    asio::any_io_executor getExecutor() const
    {
        this->requireValid();
        return m_executor;
    }
private:
    Chain(NodeSharedPtr node_sp, asio::any_io_executor executor) noexcept :
        m_node_sp(std::move(node_sp)), m_executor(std::move(executor)) {}
    void requireValid() const
    {
        if (not m_node_sp) { throw std::logic_error("an async chain has been consumed"); }
    }
    template <typename Operation>
    static Self from_operation(asio::any_io_executor executor, Operation operation)
    {
        auto node_sp = std::make_shared<Node>();
        node_sp->m_operation = std::move(operation);
        return Self {std::move(node_sp), std::move(executor)};
    }
    static asio::awaitable<void> run_node(NodeSharedPtr node_sp)
    {
        if (node_sp->isReady()) { co_return; }
        if (not node_sp->tryStart()) { throw std::logic_error("an async chain cannot run concurrently"); }
        try {
            auto result_opt = co_await node_sp->m_operation();
            node_sp->publish(std::move(result_opt).value());
        } catch (...) {
            if (not node_sp->isReady()) {
                node_sp->publish(std::unexpected(details::exception_error(std::current_exception())));
            }
        }
        node_sp->m_operation = {};
    }
    static asio::awaitable<ResultView<T>> run_owned(NodeSharedPtr node_sp)
    {
        co_await run_node(node_sp);
        const auto * result_p = &node_sp->result();
        co_return ResultView<T> {std::move(node_sp), result_p};
    }
private:
    NodeSharedPtr m_node_sp;
    asio::any_io_executor m_executor;
};

template <typename T>
Chain(T, asio::any_io_executor) -> Chain<T>;

Chain(asio::any_io_executor) -> Chain<void>;

}
