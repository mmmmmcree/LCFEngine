#include "../support/Runtime.h"
#include <asio/steady_timer.hpp>
#include <gtest/gtest.h>
#include <atomic>
#include <string>

namespace {

namespace task = lcf::async::task;
using lcf::async::testing::Runtime;
using namespace std::chrono_literals;

struct MoveOnlyValue
{
    explicit MoveOnlyValue(int value) : m_value(value) {}
    MoveOnlyValue(MoveOnlyValue &&) noexcept = default;
    MoveOnlyValue(const MoveOnlyValue &) = delete;
    int m_value;
};

asio::awaitable<void> wait_once()
{
    asio::steady_timer timer {co_await asio::this_coro::executor};
    timer.expires_after(1ms);
    co_await timer.async_wait(asio::use_awaitable);
}

TEST(Chain, RoutesWorkAndCompletionToTheirExecutors)
{
    Runtime runtime;
    const auto render_thread = std::this_thread::get_id();
    std::thread::id worker_thread;
    std::thread::id successor_thread;
    bool completed = false;
    auto chain = task::Chain {3, runtime.cpu()}
        .link([&](const int & value) {
            worker_thread = std::this_thread::get_id();
            return value + 1;
        })
        .link(runtime.control(), [&](const int & value) {
            successor_thread = std::this_thread::get_id();
            return value * 2;
        });
    int output = 0;
    std::thread::id completion_thread;
    std::move(chain).start(runtime.control(), [&](const task::Result<int> & result) {
        output = result.value();
        completion_thread = std::this_thread::get_id();
        completed = true;
    });
    runtime.pumpUntil([&] { return completed; });
    EXPECT_EQ(output, 8);
    EXPECT_NE(worker_thread, render_thread);
    EXPECT_EQ(successor_thread, render_thread);
    EXPECT_EQ(completion_thread, render_thread);
}

TEST(Chain, HandlesRetainEarlierValuesAndOmitVoidArguments)
{
    Runtime runtime;
    auto seed = task::Chain {7, runtime.cpu()};
    const task::Chain<int>::ResultHandle input_rh = seed.getResult();
    auto empty = std::move(seed).link([](const int &) {});
    auto empty_rh = empty.getResult();
    auto text = std::move(empty).link([] { return std::string {"text"}; });
    auto text_rh = text.getResult();
    auto value = std::move(text).link([](const std::string & value) { return static_cast<int>(value.size()); });
    auto tail = std::move(value).link(
        [](const int & current, const int & input, const std::string & text, const int & repeated) {
            return current + input + static_cast<int>(text.size()) + repeated;
        }, input_rh, std::move(empty_rh), text_rh, input_rh);
    auto result_rv = runtime.run(std::move(tail));
    ASSERT_TRUE(*result_rv);
    EXPECT_EQ(result_rv->value(), 22);
}

TEST(Chain, NormalizesVoidStatusAndAwaitableResults)
{
    Runtime runtime;
    auto chain = task::Chain {runtime.cpu()}
        .link([] {})
        .link([] { return std::error_code {}; })
        .link([] { return task::Result<void> {}; })
        .link(wait_once)
        .link([] { return MoveOnlyValue {42}; });
    auto result_rv = runtime.run(std::move(chain));
    ASSERT_TRUE(*result_rv);
    EXPECT_EQ(result_rv->value().m_value, 42);
}

TEST(Chain, SupportsMoveOnlyCallablesAndKeepsOrdinaryTuplesIntact)
{
    Runtime runtime;
    auto chain = task::Chain {1, runtime.cpu()}
        .link([value_up = std::make_unique<int>(4)](const int & input) { return std::tuple {input, *value_up}; })
        .link([](const std::tuple<int, int> & input) { return std::get<0>(input) + std::get<1>(input); });
    EXPECT_EQ(runtime.run(std::move(chain))->value(), 5);
}

TEST(Chain, TemporaryRunOwnsItsStateAndReleasesCompletedCaptures)
{
    Runtime runtime;
    auto token_sp = std::make_shared<int>(19);
    std::weak_ptr<int> token_wp = token_sp;
    auto chain = task::Chain {runtime.cpu()}.link([token_sp] { return *token_sp; });
    [[maybe_unused]] auto result_rh = chain.getResult();
    token_sp.reset();
    auto result_rv = runtime.run(std::move(chain));
    ASSERT_TRUE(*result_rv);
    EXPECT_EQ(result_rv->value(), 19);
    EXPECT_TRUE(token_wp.expired());

    std::optional<task::ResultView<int>> temporary_result_rv_opt;
    asio::io_context context;
    asio::co_spawn(context,
        task::Chain {23, context.get_executor()}.link([](const int & value) { return value + 1; }).run(),
        [&](std::exception_ptr exception, task::ResultView<int> result_rv) {
            if (exception) { std::rethrow_exception(exception); }
            temporary_result_rv_opt = std::move(result_rv);
        });
    context.run();
    ASSERT_TRUE(temporary_result_rv_opt);
    EXPECT_EQ((*temporary_result_rv_opt)->value(), 24);
}

TEST(Chain, ReturnedFailureSkipsSuccessors)
{
    Runtime runtime;
    const auto failure = std::make_error_code(std::errc::invalid_argument);
    std::atomic_uint forbidden_runs = 0;
    auto failed = task::Chain {runtime.cpu()}.link([&]() -> task::Result<int> { return std::unexpected(failure); });
    auto chain = std::move(failed).link([&](const int &) { ++forbidden_runs; return 0; });
    auto failed_rv = runtime.run(std::move(chain));
    ASSERT_FALSE(*failed_rv);
    EXPECT_EQ(failed_rv->error(), failure);
    EXPECT_EQ(forbidden_runs, 0u);
}

TEST(Chain, RunPropagatesExceptionsAndReleasesCaptures)
{
    Runtime runtime;
    const auto failure = std::make_error_code(std::errc::invalid_argument);
    std::atomic_uint forbidden_runs = 0;
    auto token_sp = std::make_shared<int>(19);
    std::weak_ptr<int> token_wp = token_sp;
    auto throwing = task::Chain {runtime.cpu()}.link([token_sp, failure]() -> asio::awaitable<int> {
        co_await wait_once();
        if (failure) { throw std::system_error(failure); }
        co_return *token_sp;
    });
    [[maybe_unused]] auto failed_rh = throwing.getResult();
    auto tail = std::move(throwing).link([&](const int &) { ++forbidden_runs; return 0; });
    [[maybe_unused]] auto tail_rh = tail.getResult();
    token_sp.reset();
    try {
        runtime.run(std::move(tail));
        FAIL() << "run() must propagate the task exception";
    } catch (const std::system_error & error) {
        EXPECT_EQ(error.code(), failure);
    }
    EXPECT_EQ(forbidden_runs, 0u);
    EXPECT_TRUE(token_wp.expired());

    auto unknown = task::Chain {runtime.cpu()}.link([] { throw std::runtime_error("failure"); });
    EXPECT_THROW(runtime.run(std::move(unknown)), std::runtime_error);
}

TEST(Chain, StartConvertsExceptionsAtTheCompletionBoundary)
{
    Runtime runtime;
    const auto control_thread = std::this_thread::get_id();
    const auto failure = std::make_error_code(std::errc::invalid_argument);
    for (bool system_failure : {true, false}) {
        unsigned completions = 0;
        auto chain = task::Chain {runtime.cpu()}.link([failure, system_failure] {
            if (system_failure) { throw std::system_error(failure); }
            throw std::runtime_error("failure");
        });
        std::move(chain).start(runtime.control(), [&](const task::Result<void> & result) {
            EXPECT_EQ(std::this_thread::get_id(), control_thread);
            EXPECT_FALSE(result);
            EXPECT_EQ(result.error(), system_failure ? failure : std::make_error_code(std::errc::io_error));
            ++completions;
        });
        runtime.pumpUntil([&] { return completions != 0; });
        EXPECT_EQ(completions, 1u);
    }
}

TEST(Chain, CompletionExceptionsEscapeToTheContextCaller)
{
    asio::io_context context;
    auto chain = task::Chain {context.get_executor()}.link([] {});
    std::move(chain).start([](const task::Result<void> &) { throw std::runtime_error("completion"); });
    EXPECT_THROW(context.run(), std::runtime_error);
}

}
