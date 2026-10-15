#include "../support/Runtime.h"
#include <gtest/gtest.h>
#include <string>

#if LCF_ASYNC_USE_MODULES
namespace lcf::async::testing {

task::Chain<int> make_header_chain(asio::any_io_executor executor);
task::Flow<int> share_header_chain(task::Chain<int> chain);

}
#endif

namespace {

namespace task = lcf::async::task;
using lcf::async::testing::Runtime;

struct IntegerTask
{
    int operator()(const int & value) const { return value + 1; }
};

struct DependencyTask
{
    int operator()(const int & a, const int & b) const { return a + b; }
};

struct WrongInputTask
{
    int operator()(std::string &) const { return 0; }
};

struct FakeHandle
{
    using value_type = int;
};

template <typename Source, typename Function, typename... Handles>
concept linkable_c = requires(Source && source, Function function, Handles... handles) {
    std::forward<Source>(source).link(std::move(function), handles...);
};

template <typename... Sources>
concept mergeable_c = requires(Sources &&... sources) {
    task::merge(std::forward<Sources>(sources)...);
};

static_assert(linkable_c<task::Chain<int>, IntegerTask>);
static_assert(not linkable_c<task::Chain<int> &, IntegerTask>);
static_assert(not linkable_c<task::Chain<int>, WrongInputTask>);
static_assert(linkable_c<const task::Flow<int> &, IntegerTask>);
static_assert(linkable_c<task::Chain<int>, DependencyTask, task::Chain<int>::ResultHandle>);
static_assert(not linkable_c<task::Chain<int>, DependencyTask, FakeHandle>);
static_assert(not linkable_c<task::Chain<int>, DependencyTask, int>);
static_assert(linkable_c<task::Chain<int>, IntegerTask, task::Chain<void>::ResultHandle>);
static_assert(mergeable_c<task::Chain<int>, task::Flow<int> &>);
static_assert(not mergeable_c<task::Chain<int> &, task::Flow<int> &>);
static_assert(not mergeable_c<const task::Chain<int>, task::Flow<int>>);
static_assert(not mergeable_c<int, task::Flow<int>>);
static_assert(not mergeable_c<task::Flow<int>>);
static_assert(not std::copy_constructible<task::Chain<int>>);
static_assert(std::copy_constructible<task::Flow<int>>);
static_assert(std::same_as<decltype(std::declval<const task::Flow<int> &>().link(IntegerTask {})), task::Chain<int>>);
struct ErrorCodeTask
{
    task::Result<std::error_code> operator()(const int &) const { return {}; }
};

struct NestedExpectedTask
{
    task::Result<task::Result<int>> operator()(const int &) const { return {}; }
};

static_assert(std::same_as<decltype(std::declval<task::Chain<int>>().link(ErrorCodeTask {})),
    task::Chain<std::error_code>>);
static_assert(std::same_as<decltype(std::declval<task::Chain<int>>().link(NestedExpectedTask {})),
    task::Chain<task::Result<int>>>);

TEST(Contracts, ConsumedHandlesAndUnreadyResultsAreRejected)
{
    Runtime runtime;
    auto chain = task::Chain {3, runtime.cpu()}.link(IntegerTask {});
    auto handle = chain.getResult();
    EXPECT_FALSE(handle.isReady());
    EXPECT_THROW(static_cast<void>(handle.getResult()), std::logic_error);
    auto flow = task::as_flow(std::move(chain));
    EXPECT_THROW(static_cast<void>(std::move(chain).run()), std::logic_error);
    EXPECT_THROW(static_cast<void>(flow.getResult()), std::logic_error);
    auto result_sp = runtime.run(flow);
    EXPECT_EQ(result_sp->value(), 4);
    bool completed = false;
    std::move(flow).start(runtime.control(), [&](const auto &) { completed = true; });
    EXPECT_THROW(static_cast<void>(flow.run()), std::logic_error);
    runtime.pumpUntil([&] { return completed; });
}

TEST(Contracts, OuterExpectedPreservesErrorCodeAndNestedExpectedPayloads)
{
    Runtime runtime;
    const auto code = std::make_error_code(std::errc::invalid_argument);
    auto first = task::Chain {runtime.cpu()}.link([&]() -> task::Result<std::error_code> {
        return task::Result<std::error_code> {std::in_place, code};
    });
    auto second = task::Chain {runtime.cpu()}.link([&]() -> task::Result<task::Result<int>> {
        return task::Result<task::Result<int>> {std::in_place, std::unexpected(code)};
    });
    auto joined = task::merge(std::move(first), std::move(second))
        .link([&](const std::error_code & value, const task::Result<int> & inner) {
            return value == code and not inner and inner.error() == code;
        });
    auto result_sp = runtime.run(std::move(joined));
    ASSERT_TRUE(*result_sp);
    EXPECT_TRUE(result_sp->value());
}

#if LCF_ASYNC_USE_MODULES
TEST(Contracts, HeaderAndModuleShareTaskTypes)
{
    Runtime runtime;
    auto chain = lcf::async::testing::make_header_chain(runtime.cpu())
        .link([](const int & value) { return value + 1; });
    auto shared = lcf::async::testing::share_header_chain(std::move(chain));
    auto joined = task::merge(shared, shared.link([](const int & value) { return value + 1; }))
        .link([](const int & a, const int & b) { return a + b; });
    auto result_sp = runtime.run(std::move(joined));
    ASSERT_TRUE(*result_sp);
    EXPECT_EQ(result_sp->value(), 85);
}
#endif

}
