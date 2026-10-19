#include <async/core/Awaitable.h>
#include <async/core/Executor.h>
#include <async/core/Context.h>
#include <mutex>
#include <asio.hpp>
#include <asio/experimental/concurrent_channel.hpp>
#include <expected>
#include <asio/io_context.hpp>
#include <gtest/gtest.h>
#include <memory>

#include <async/task.h>

namespace {

namespace task = lcf::async::task;

/* # Async sandbox */

/* ## Chains and junctions */

/*
 * ### Link earlier results and join independently started chains
 *
 * - **Objective:** Exercise public Chain and ChainJunction operations in an editable example.
 * - **Input:** A move-only seed containing `7`; an earlier-result token; one io_context executor.
 * - **Procedure:** Link the seed into a value `14`; create a branch adding `1`;
 *   join the branch and its source; start all three chains and run the context.
 * - **Expected:** Source value `14`, branch value `15`, joined value `29`;
 *   all three completion callbacks run successfully.
 */
void link_and_join_chains();
TEST(Sandbox, LinksAndJoinsChains)
{
    link_and_join_chains();
}

/* ## Test implementations */

void link_and_join_chains()
{
    asio::io_context context;
    auto executor = context.get_executor();
    auto source = task::Chain {std::make_unique<int>(7), executor};
    auto original_token = source.getResultToken();
    auto left = std::move(source)
        .link([](const std::unique_ptr<int> & value) { return *value; })
        .link([](const int & value, const std::unique_ptr<int> & input) { return value + *input; }, original_token);
    auto left_token = left.getResultToken();
    auto shared = task::merge(left_token);
    auto branch = shared.link(executor, [](const int & value) { return value + 1; });
    auto joined = task::merge(branch.getResultToken(), left_token)
        .link(executor, [](const int & a, const int & b) { return a + b; });
    unsigned completions = 0;
    std::move(left).start(executor, [&](const task::Result<int> & result) {
        ASSERT_TRUE(result);
        EXPECT_EQ(result.value(), 14);
        ++completions;
    });
    std::move(branch).start(executor, [&](const task::Result<int> & result) {
        ASSERT_TRUE(result);
        EXPECT_EQ(result.value(), 15);
        ++completions;
    });
    std::move(joined).start(executor, [&](const task::Result<int> & result) {
        ASSERT_TRUE(result);
        EXPECT_EQ(result.value(), 29);
        ++completions;
    });
    context.run();
    EXPECT_EQ(completions, 3u);
}

}
