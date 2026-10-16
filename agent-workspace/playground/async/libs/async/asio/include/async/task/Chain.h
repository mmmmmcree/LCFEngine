#pragma once

#include "common.h"
#include "error_handling.h"
#include <asio/co_spawn.hpp>
#include <atomic>
#include <cassert>

namespace lcf::async::task {

namespace details {

template <typename Handle>
concept chain_result_handle_c = requires {
    typename std::remove_cvref_t<Handle>::value_type;
    typename Chain<typename std::remove_cvref_t<Handle>::value_type>::ResultHandle;
} and std::same_as<std::remove_cvref_t<Handle>,
    typename Chain<typename std::remove_cvref_t<Handle>::value_type>::ResultHandle>;

}

template <details::result_value_c T>
class Chain
{
    template <details::result_value_c> friend class Chain;
    template <details::result_value_c> friend class Flow;
    using Self = Chain;
    struct Node
    {
        using Operation = std::move_only_function<asio::awaitable<details::ResultPlaceholder<T>>()>;
        void publish(Result<T> result)
        {
            m_result_placeholder.emplace(std::move(result));
            m_ready.store(true, std::memory_order_release);
        }
        bool isReady() const noexcept { return m_ready.load(std::memory_order_acquire); }
        const Result<T> & result() const noexcept { return *m_result_placeholder; }
        Operation m_operation;
        details::ResultPlaceholder<T> m_result_placeholder;
        std::atomic_bool m_ready = false;
    };
    using NodeSharedPtr = std::shared_ptr<Node>;
public:
    using value_type = T;
    // Dependency token; result access belongs to Chain.
    class ResultHandle
    {
        template <details::result_value_c> friend class Chain;
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
    private:
        explicit ResultHandle(NodeSharedPtr node_sp) noexcept : m_node_sp(std::move(node_sp)) {}
    private:
        NodeSharedPtr m_node_sp;
    };
public:
    ~Chain() noexcept = default;
    template <typename Value>
    requires (not std::is_void_v<T> and std::same_as<std::remove_cvref_t<Value>, T> and std::constructible_from<T, Value>)
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
        using Value = details::function_value_t<Function, T, typename Handles::value_type...>;
        auto input_sp = std::move(m_node_sp);
        auto dependency_sps = std::tuple {std::move(dependencies.m_node_sp)...};
        return Chain<Value>::from_operation(executor,
            [input_sp = std::move(input_sp), dependency_sps = std::move(dependency_sps),
                executor, function = std::move(function)]() mutable -> asio::awaitable<details::ResultPlaceholder<Value>> {
                co_await run_node(input_sp);
                const auto & input_result = input_sp->result();
                if (not input_result) { co_return Result<Value> {std::unexpected(input_result.error())}; }
                auto arguments = std::apply([&](const auto &... dependency_sp) {
                    assert((... and dependency_sp->isReady()) and
                        "a ResultHandle dependency must already be completed; link() only waits for its direct predecessor, "
                        "so handles from independent unfinished chains cannot be used as dependencies");
                    return std::tuple_cat(details::result_to_arguments(input_result),
                        details::result_to_arguments(dependency_sp->result())...);
                }, dependency_sps);
                co_return co_await asio::co_spawn(executor, details::invoke_task(std::move(function), std::move(arguments)), asio::use_awaitable);
            });
    }
    template <typename Function, typename Handler, details::chain_result_handle_c... Handles>
    requires (details::task_function_c<Function, T, typename Handles::value_type...>
        and details::error_handler_c<Handler, details::function_value_t<Function, T, typename Handles::value_type...>>)
    auto link(Function function, ErrorHandler<Handler> error_handler, Handles... dependencies) &&
    {
        return std::move(*this).link(m_executor, std::move(function), std::move(error_handler), std::move(dependencies)...);
    }
    template <typename Function, typename Handler, details::chain_result_handle_c... Handles>
    requires (details::task_function_c<Function, T, typename Handles::value_type...>
        and details::error_handler_c<Handler, details::function_value_t<Function, T, typename Handles::value_type...>>)
    auto link(asio::any_io_executor executor, Function function, ErrorHandler<Handler> error_handler, Handles... dependencies) &&
    {
        return std::move(*this).link(
            std::move(executor),
            details::ErrorHandledTask {std::move(function),
            std::move(error_handler.m_function)},
            std::move(dependencies)...);
    }
    // Requires an unconsumed chain. Task exceptions propagate without publishing a Result.
    asio::awaitable<ResultView<T>> run() && { return run_owned(std::move(m_node_sp)); }
    template <typename Completion>
    requires details::completion_c<Completion, T>
    void start(asio::any_io_executor executor, Completion completion) &&
    {
        auto operation = std::move(*this).run();
        asio::co_spawn(executor, std::move(operation),
            [completion = std::move(completion)](std::exception_ptr exception, ResultView<T> result_rv) mutable {
                if (exception) {
                    const Result<T> result {std::unexpected(details::exception_error(exception))};
                    std::invoke(completion, result);
                    return;
                }
                std::invoke(completion, *result_rv);
            });
    }
    template <typename Completion>
    requires details::completion_c<Completion, T>
    void start(Completion completion) && { std::move(*this).start(m_executor, std::move(completion)); }
    ResultHandle getResult() const noexcept { return ResultHandle {m_node_sp}; }
    asio::any_io_executor getExecutor() const noexcept { return m_executor; }
private:
    Chain(NodeSharedPtr node_sp, asio::any_io_executor executor) noexcept :
        m_node_sp(std::move(node_sp)), m_executor(std::move(executor)) {}
    template <typename Operation>
    static Self from_operation(asio::any_io_executor executor, Operation operation)
    {
        auto node_sp = std::make_shared<Node>();
        node_sp->m_operation = std::move(operation);
        return Self {std::move(node_sp), std::move(executor)};
    }
    static asio::awaitable<void> run_node(NodeSharedPtr node_sp)
    {
        assert(node_sp and "a consumed or moved-from Chain cannot be executed; "
            "link(), run(), start(), as_flow(), and merge() transfer ownership from the source Chain");
        if (node_sp->isReady()) { co_return; }
        auto operation = std::exchange(node_sp->m_operation, {});
        auto result_opt = co_await operation();
        node_sp->publish(std::move(*result_opt));
    }
    static asio::awaitable<ResultView<T>> run_owned(NodeSharedPtr node_sp)
    {
        co_await run_node(node_sp);
        const auto * result_p = &node_sp->result();
        co_return make_result_view(std::move(node_sp), result_p);
    }
private:
    NodeSharedPtr m_node_sp;
    asio::any_io_executor m_executor;
};

template <typename T>
Chain(T, asio::any_io_executor) -> Chain<T>;

Chain(asio::any_io_executor) -> Chain<void>;

}
