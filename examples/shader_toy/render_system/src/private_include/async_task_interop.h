#pragma once

#include "AsyncTaskFlow.h"

namespace lcf::async::task {

template <typename T>
Flow<T> as_flow(Chain<T> && chain, asio::any_io_executor executor)
{
    auto operation = std::move(chain).run();
    return details::FlowAccess::make<T>(std::move(executor),
        [operation = std::move(operation)]() mutable -> asio::awaitable<ResultView<T>> {
            co_return co_await std::move(operation);
        });
}

template <typename T>
Flow<T> as_flow(Chain<T> && chain)
{
    auto executor = chain.getExecutor();
    return as_flow(std::move(chain), std::move(executor));
}

namespace details {

template <typename T>
Flow<T> to_flow(Chain<T> && chain) { return as_flow(std::move(chain)); }

template <typename T>
Flow<T> to_flow(Flow<T> flow)
{
    flow.getExecutor();
    return flow;
}

template <typename... Values, std::size_t... Indices>
asio::awaitable<Result<MergedValues<Values...>>> merge_results(
    std::tuple<Flow<Values>...> inputs, std::index_sequence<Indices...>)
{
    auto executor = co_await asio::this_coro::executor;
    (FlowAccess::start(std::get<Indices>(inputs), executor), ...);
    // Start every input before awaiting any one input; retain all results until the join completes.
    auto results = std::tuple<ResultView<Values>...> {co_await std::get<Indices>(inputs).run()...};
    std::optional<std::error_code> failure_opt;
    std::apply([&](const auto &... result_sp) {
        ([&] {
            if (not failure_opt and not *result_sp) { failure_opt = result_sp->error(); }
        }(), ...);
    }, results);
    if (failure_opt) { co_return std::unexpected(*failure_opt); }
    co_return Result<MergedValues<Values...>> {std::in_place, MergedValues<Values...> {std::move(results)}};
}

}

template <typename... Sources>
    requires (sizeof...(Sources) >= 2u and (details::source_c<Sources> and ...))
auto merge(Sources &&... sources)
{
    using Value = details::MergedValues<details::source_value_t<Sources>...>;
    auto inputs = std::tuple {details::to_flow(std::forward<Sources>(sources))...};
    auto executor = std::get<0>(inputs).getExecutor();
    return details::FlowAccess::make<Value>(std::move(executor),
        [inputs = std::move(inputs)]() mutable -> asio::awaitable<Result<Value>> {
            co_return co_await details::merge_results(std::move(inputs), std::index_sequence_for<Sources...> {});
        });
}

}
