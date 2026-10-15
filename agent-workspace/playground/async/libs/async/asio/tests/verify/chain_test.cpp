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
    const task::Chain<int>::ResultHandle input = seed.getResult();
    auto empty = std::move(seed).link([](const int &) {});
    auto empty_handle = empty.getResult();
    auto text = std::move(empty).link([] { return std::string {"text"}; });
    auto text_handle = text.getResult();
    auto value = std::move(text).link([](const std::string & value) { return static_cast<int>(value.size()); });
    auto tail = std::move(value).link(
        [](const int & current, const int & input, const std::string & text, const int & repeated) {
            return current + input + static_cast<int>(text.size()) + repeated;
        }, input, std::move(empty_handle), text_handle, input);
    EXPECT_FALSE(empty_handle.isReady());
    auto result_sp = runtime.run(std::move(tail));
    ASSERT_TRUE(*result_sp);
    EXPECT_EQ(result_sp->value(), 22);
    EXPECT_EQ(input.getResult().value(), 7);
    EXPECT_EQ(text_handle.getResult().value(), "text");
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
    auto result_sp = runtime.run(std::move(chain));
    ASSERT_TRUE(*result_sp);
    EXPECT_EQ(result_sp->value().m_value, 42);
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
    auto handle = chain.getResult();
    token_sp.reset();
    auto result_sp = runtime.run(std::move(chain));
    ASSERT_TRUE(*result_sp);
    EXPECT_EQ(result_sp->value(), 19);
    EXPECT_EQ(handle.getResult().value(), 19);
    EXPECT_TRUE(token_wp.expired());

    std::optional<task::ResultView<int>> temporary_result_opt;
    asio::io_context context;
    asio::co_spawn(context,
        task::Chain {23, context.get_executor()}.link([](const int & value) { return value + 1; }).run(),
        [&](std::exception_ptr exception, task::ResultView<int> result_sp) {
            if (exception) { std::rethrow_exception(exception); }
            temporary_result_opt = std::move(result_sp);
        });
    context.run();
    ASSERT_TRUE(temporary_result_opt);
    EXPECT_EQ((*temporary_result_opt)->value(), 24);
}

TEST(Chain, ConvertsExceptionsAndSkipsFailedSuccessors)
{
    Runtime runtime;
    const auto failure = std::make_error_code(std::errc::invalid_argument);
    std::atomic_uint forbidden_runs = 0;
    auto chain = task::Chain {runtime.cpu()}
        .link([&]() -> task::Result<int> { return std::unexpected(failure); })
        .link([&](const int &) { ++forbidden_runs; return 0; });
    auto failed_sp = runtime.run(std::move(chain));
    ASSERT_FALSE(*failed_sp);
    EXPECT_EQ(failed_sp->error(), failure);
    EXPECT_EQ(forbidden_runs, 0u);

    auto throwing = task::Chain {runtime.cpu()}.link([&]() -> MoveOnlyValue { throw std::system_error(failure); });
    auto exception_sp = runtime.run(std::move(throwing));
    ASSERT_FALSE(*exception_sp);
    EXPECT_EQ(exception_sp->error(), failure);

    auto unknown = task::Chain {runtime.cpu()}.link([] { throw std::runtime_error("failure"); });
    auto unknown_sp = runtime.run(std::move(unknown));
    ASSERT_FALSE(*unknown_sp);
    EXPECT_EQ(unknown_sp->error(), std::errc::io_error);
}

}
