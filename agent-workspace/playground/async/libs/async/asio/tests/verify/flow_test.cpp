#include "../support/Runtime.h"
#include <asio/steady_timer.hpp>
#include <gtest/gtest.h>
#include <atomic>
#include <array>

namespace {

namespace task = lcf::async::task;
using lcf::async::testing::Runtime;
using namespace std::chrono_literals;

asio::awaitable<int> delayed_value(int value)
{
    asio::steady_timer timer {co_await asio::this_coro::executor};
    timer.expires_after(1ms);
    co_await timer.async_wait(asio::use_awaitable);
    co_return value;
}

TEST(Flow, StartsBothChainsBeforeAwaitingEither)
{
    Runtime runtime;
    std::atomic_uint started = 0;
    std::array<std::thread::id, 2> threads;
    auto work = [&](const int & index) -> task::Result<int> {
        threads[index] = std::this_thread::get_id();
        ++started;
        const auto deadline = std::chrono::steady_clock::now() + 3s;
        while (started.load() != 2) {
            if (std::chrono::steady_clock::now() >= deadline) { return std::unexpected(std::make_error_code(std::errc::timed_out)); }
            std::this_thread::yield();
        }
        return index + 1;
    };
    auto image = task::Chain {0, runtime.cpu()}.link(work);
    auto shader = task::Chain {1, runtime.cpu()}.link(work);
    auto joined = task::merge(std::move(image), std::move(shader))
        .link([](const int & a, const int & b) { return a + b; });
    auto result_rv = runtime.run(std::move(joined));
    ASSERT_TRUE(*result_rv);
    EXPECT_EQ(result_rv->value(), 3);
    EXPECT_NE(threads[0], threads[1]);
}

TEST(Flow, SharedStartsRaceWithoutRepeatingTheSource)
{
    Runtime runtime;
    for (unsigned iteration = 0; iteration < 100; ++iteration) {
        std::atomic_uint source_runs = 0;
        std::atomic_uint completions = 0;
        std::atomic_uint failures = 0;
        auto source = task::as_flow(task::Chain {7, runtime.cpu()}.link([&](const int & value) {
            ++source_runs;
            return std::make_unique<int>(value);
        }));
        auto a = task::as_flow(source.link([](const std::unique_ptr<int> & value) { return *value + 1; }));
        auto b = task::as_flow(source.link([](const std::unique_ptr<int> & value) { return delayed_value(*value + 2); }));
        auto started_a = a;
        auto started_b = b;
        std::move(started_a).start(runtime.cpu(), [&](const task::Result<int> & result) {
            if (not result or result.value() != 8) { ++failures; }
            ++completions;
        });
        std::move(started_b).start(runtime.control(), [&](const task::Result<int> & result) {
            if (not result or result.value() != 9) { ++failures; }
            ++completions;
        });
        auto joined = task::merge(a, b).link([](const int & a, const int & b) { return a + b; });
        auto result_rv = runtime.run(std::move(joined));
        runtime.pumpUntil([&] { return completions == 2; });
        ASSERT_TRUE(*result_rv);
        EXPECT_EQ(result_rv->value(), 17);
        EXPECT_EQ(failures, 0u);
        EXPECT_EQ(source_runs, 1u);
        EXPECT_EQ(*runtime.run(source)->value(), 7);
        EXPECT_EQ(source_runs, 1u);
    }
}

TEST(Flow, AdapterRetainsTheExactMoveOnlyResult)
{
    Runtime runtime;
    auto chain = task::Chain {runtime.cpu()}.link([] { return std::make_unique<int>(42); });
    auto original_rh = chain.getResult();
    auto source = task::as_flow(std::move(chain));
    auto result_rv = runtime.run(source);
    ASSERT_TRUE(*result_rv);
    EXPECT_EQ(*result_rv->value(), 42);
    EXPECT_EQ(&source.getResult(), result_rv.get());
    auto checked = source.link([](const std::unique_ptr<int> & value_up) { return &value_up; })
        .link([](const std::unique_ptr<int> * const & value_p, const std::unique_ptr<int> & original_up) {
            return value_p == &original_up;
        }, original_rh);
    EXPECT_TRUE(runtime.run(std::move(checked))->value());
}

TEST(Flow, NestedDuplicateAndVoidInputsPreserveArgumentOrder)
{
    Runtime runtime;
    auto source = task::as_flow(task::Chain {5, runtime.cpu()}.link([](const int & value) { return value; }));
    auto empty = task::Chain {runtime.cpu()}.link([] {});
    auto other = task::Chain {2, runtime.cpu()}.link([](const int & value) { return value; });
    auto joined = task::merge(task::merge(source, std::move(empty), source), std::move(other))
        .link([](const int & a, const int & b, const int & c) { return 100 * a + 10 * b + c; });
    EXPECT_EQ(runtime.run(std::move(joined))->value(), 552);
}

TEST(Flow, MergeDrainsSiblingsBeforeReportingFailure)
{
    Runtime runtime;
    const auto failure = std::make_error_code(std::errc::invalid_argument);
    std::atomic_bool drained = false;
    std::atomic_uint forbidden_runs = 0;
    auto failed = task::Chain {runtime.cpu()}.link([&]() -> task::Result<int> { return std::unexpected(failure); });
    auto sibling = task::Chain {2, runtime.cpu()}
        .link([](const int & value) { return delayed_value(value); })
        .link([&](const int & value) { drained = true; return value; });
    auto joined = task::merge(std::move(failed), std::move(sibling))
        .link([&](const int &, const int &) { ++forbidden_runs; });
    auto result_rv = runtime.run(std::move(joined));
    ASSERT_FALSE(*result_rv);
    EXPECT_EQ(result_rv->error(), failure);
    EXPECT_TRUE(drained);
    EXPECT_EQ(forbidden_runs, 0u);
}

TEST(Flow, MergeConvertsChainExceptionsAndDrainsSiblings)
{
    Runtime runtime;
    const auto failure = std::make_error_code(std::errc::invalid_argument);
    std::atomic_uint source_runs = 0;
    std::atomic_bool drained = false;
    std::atomic_uint forbidden_runs = 0;
    auto failed = task::as_flow(task::Chain {runtime.cpu()}.link([&]() -> int {
        ++source_runs;
        throw std::system_error(failure);
    }));
    auto sibling = task::Chain {2, runtime.cpu()}
        .link([](const int & value) { return delayed_value(value); })
        .link([&](const int & value) { drained = true; return value; });
    auto joined = task::merge(failed, std::move(sibling))
        .link([&](const int &, const int &) { ++forbidden_runs; });
    auto result_rv = runtime.run(std::move(joined));
    ASSERT_FALSE(*result_rv);
    EXPECT_EQ(result_rv->error(), failure);
    EXPECT_TRUE(drained);
    EXPECT_EQ(forbidden_runs, 0u);
    auto repeated_rv = runtime.run(failed);
    ASSERT_FALSE(*repeated_rv);
    EXPECT_EQ(repeated_rv->error(), failure);
    EXPECT_EQ(source_runs, 1u);
}

TEST(Flow, FailureOrderPreservesUnexpectedWithZeroErrorCode)
{
    Runtime runtime;
    auto first = task::Chain {runtime.cpu()}.link([]() -> task::Result<int> { return std::unexpected(std::error_code {}); });
    auto second = task::Chain {runtime.cpu()}.link([]() -> task::Result<int> {
        return std::unexpected(std::make_error_code(std::errc::invalid_argument));
    });
    auto result_rv = runtime.run(task::merge(std::move(first), std::move(second)));
    ASSERT_FALSE(*result_rv);
    EXPECT_FALSE(result_rv->error());
}

}
