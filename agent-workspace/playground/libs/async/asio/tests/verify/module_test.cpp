#include <Runtime.h>
#include <gtest/gtest.h>

#if LCF_ASYNC_USE_MODULES
namespace lcf::async::testing {

task::Chain<int> make_header_chain(lcf::async::Executor executor);
task::ChainJunction<int> describe_header_chain(const task::Chain<int> & chain);

}
#endif

namespace {
namespace task = lcf::async::task;
using lcf::async::testing::Runtime;

/* ## Header and module compatibility */

#if LCF_ASYNC_USE_MODULES

/*
 * ### Header and module consumers exchange chains, junctions, and tokens
 *
 * - **Objective:** Type compatibility across header-built helpers and module consumers.
 * - **Input:** A header helper creating a chain from seed `40` with one increment; a header helper
 *   describing it as a junction; module-side increment and dependency tasks.
 * - **Procedure:** Extend the header chain and its junction in the module consumer; start the chain
 *   and branch; join their result tokens.
 * - **Expected:** Successful final result with value `85`.
 * - **Failure hints:** Check `modules/async.cppm`, `async/task.h`, and `header_bridge.cpp` for duplicate type identities or missing exports before inspecting runtime execution.
 */
void header_and_module_share_task_types();
TEST(Contracts, HeaderAndModuleShareTaskTypes)
{
    header_and_module_share_task_types();
}

#endif


/* ### Header and module compatibility */

#if LCF_ASYNC_USE_MODULES

void header_and_module_share_task_types()
{
    Runtime runtime;
    auto chain = lcf::async::testing::make_header_chain(runtime.cpu()).link([](const int & value) { return value + 1; });
    auto shared = lcf::async::testing::describe_header_chain(chain);
    auto branch = shared.link(runtime.cpu(), [](const int & value) { return value + 1; });
    auto joined = task::merge(chain.getResultToken(), branch.getResultToken()).link(runtime.cpu(), [](const int & a, const int & b) { return a + b; });
    runtime.start(std::move(chain));
    runtime.start(std::move(branch));
    auto result_rh = runtime.run(std::move(joined));
    ASSERT_TRUE(*result_rh);
    EXPECT_EQ(result_rh->value(), 85);
}

#endif

}
