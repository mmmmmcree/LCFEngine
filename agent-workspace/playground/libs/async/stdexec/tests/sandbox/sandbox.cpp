#include <async/core/Awaitable.h>
#include <async/core/Executor.h>
#include <async/core/Context.h>
#include <Runtime.h>
#include <gtest/gtest.h>

namespace {

/*
 * Objective: demonstrate independently started sources and a join.
 * Input: worker tasks producing 20 and 22, with a real asynchronous yield.
 * Procedure: start both sources and pump the join on the control context.
 * Expected: one successful completion containing 42.
 * Failure hints: inspect source registration and notification if the join stalls.
 */
void independent_sources_join();
TEST(StdexecSandbox, IndependentSourcesJoin) { independent_sources_join(); }

void independent_sources_join()
{
    namespace task = lcf::async::task;
    lcf::async::testing::Runtime runtime;
    auto first = task::Chain {20, runtime.cpu()}.link([](const int & value) -> lcf::async::Awaitable<int> {
        co_await lcf::async::testing::yield();
        co_return value;
    });
    auto second = task::Chain {22, runtime.cpu()}.link([](const int & value) { return value; });
    auto joined = task::merge(first.getResultToken(), second.getResultToken())
        .link(runtime.control(), [](const int & first, const int & second) { return first + second; });
    runtime.start(std::move(first));
    runtime.start(std::move(second));
    const auto result_rh = runtime.run(std::move(joined));
    ASSERT_TRUE(*result_rh);
    EXPECT_EQ(result_rh->value(), 42);
}

}
