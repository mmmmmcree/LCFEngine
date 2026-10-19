#include <Runtime.h>
#include <gtest/gtest.h>

namespace lcf::async::testing {

task::Chain<int> make_module_chain(lcf::async::Executor executor);
task::ChainJunction<int> describe_module_chain(const task::Chain<int> & chain);

}

namespace {

/*
 * Objective: module consumers in different translation units exchange public types.
 * Input: a chain and junction produced in module_bridge.cpp.
 * Procedure: extend and start both from this translation unit and join their tokens.
 * Expected: the original and branched result sum to 83.
 * Failure hints: inspect module exports and type attachment before scheduling.
 */
void module_consumers_exchange_types();
TEST(StdexecModules, ConsumersExchangeTypes) { module_consumers_exchange_types(); }

void module_consumers_exchange_types()
{
    namespace task = lcf::async::task;
    lcf::async::testing::Runtime runtime;
    auto source = lcf::async::testing::make_module_chain(runtime.cpu());
    const auto junction = lcf::async::testing::describe_module_chain(source);
    auto branch = junction.link(runtime.control(), [](const int & value) { return value + 1; });
    auto final = task::merge(source.getResultToken(), branch.getResultToken())
        .link(runtime.cpu(), [](const int & source, const int & branch) { return source + branch; });
    runtime.start(std::move(source));
    runtime.start(std::move(branch));
    const auto result_rh = runtime.run(std::move(final));
    ASSERT_TRUE(*result_rh);
    EXPECT_EQ(result_rh->value(), 83);
}

}
