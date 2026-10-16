#include "../support/Runtime.h"
#include <gtest/gtest.h>
#include <memory>
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

template <typename Handle>
concept result_readable_c = requires(const Handle & result_rh) {
    result_rh.getResult();
};

template <typename Handle>
concept readiness_queryable_c = requires(const Handle & result_rh) {
    result_rh.isReady();
};

struct RecoverInteger
{
    task::Result<int> operator()(const task::ErrorContext &) const { return task::use_value(42); }
};

struct WrongRecoveryValue
{
    task::Result<std::string> operator()(const task::ErrorContext &) const { return task::use_value(std::string {"wrong"}); }
};

struct MissingRecoveryDecision
{
    void operator()(const task::ErrorContext &) const {}
};

static_assert(linkable_c<task::Chain<int>, IntegerTask, task::ErrorHandler<RecoverInteger>>);
static_assert(linkable_c<task::Chain<int>, DependencyTask,
    task::ErrorHandler<RecoverInteger>, task::Chain<int>::ResultHandle>);
static_assert(linkable_c<const task::Flow<int> &, IntegerTask, task::ErrorHandler<RecoverInteger>>);
static_assert(not linkable_c<task::Chain<int>, IntegerTask, task::ErrorHandler<WrongRecoveryValue>>);
static_assert(not linkable_c<task::Chain<int>, IntegerTask, task::ErrorHandler<MissingRecoveryDecision>>);
static_assert(not linkable_c<const task::Flow<int> &, IntegerTask, task::ErrorHandler<WrongRecoveryValue>>);
static_assert(std::copyable<task::Chain<int>::ResultHandle>);
static_assert(std::copyable<task::Chain<void>::ResultHandle>);
static_assert(not result_readable_c<task::Chain<int>::ResultHandle>);
static_assert(not result_readable_c<task::Chain<void>::ResultHandle>);
static_assert(not readiness_queryable_c<task::Chain<int>::ResultHandle>);
static_assert(not readiness_queryable_c<task::Chain<void>::ResultHandle>);
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

TEST(Contracts, FlowChecksUnreadyResultsAndConsumedHandles)
{
    Runtime runtime;
    auto chain = task::Chain {3, runtime.cpu()}.link(IntegerTask {});
    auto flow = task::as_flow(std::move(chain));
    EXPECT_THROW(static_cast<void>(flow.getResult()), std::logic_error);
    auto result_rv = runtime.run(flow);
    EXPECT_EQ(result_rv->value(), 4);
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
    auto result_rv = runtime.run(std::move(joined));
    ASSERT_TRUE(*result_rv);
    EXPECT_TRUE(result_rv->value());
}

TEST(Contracts, ResultViewsOwnImmutableSuccessAndFailureResults)
{
    auto result_rv = task::make_result_view<std::unique_ptr<int>>(std::in_place, std::make_unique<int>(42));
    static_assert(std::same_as<decltype(*result_rv), const task::Result<std::unique_ptr<int>> &>);
    ASSERT_TRUE(*result_rv);
    EXPECT_EQ(*result_rv->value(), 42);
    auto empty_rv = task::make_result_view<void>();
    EXPECT_TRUE(*empty_rv);
    const auto failure = std::make_error_code(std::errc::invalid_argument);
    auto failed_rv = task::make_result_view<int>(std::unexpected(failure));
    ASSERT_FALSE(*failed_rv);
    EXPECT_EQ(failed_rv->error(), failure);
}

TEST(Contracts, ResultViewsRetainAliasedOwnersWithoutCopyingResults)
{
    struct Owner
    {
        task::Result<std::unique_ptr<int>> m_result {std::in_place, std::make_unique<int>(42)};
    };
    auto owner_sp = std::make_shared<Owner>();
    std::weak_ptr<Owner> owner_wp = owner_sp;
    const auto * result_p = &owner_sp->m_result;
    static_assert(noexcept(task::make_result_view(owner_sp, result_p)));
    auto result_rv = task::make_result_view(std::move(owner_sp), result_p);
    EXPECT_FALSE(owner_sp);
    EXPECT_FALSE(owner_wp.expired());
    EXPECT_EQ(result_rv.get(), result_p);
    ASSERT_TRUE(*result_rv);
    EXPECT_EQ(*result_rv->value(), 42);
    result_rv.reset();
    EXPECT_TRUE(owner_wp.expired());
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
    auto result_rv = runtime.run(std::move(joined));
    ASSERT_TRUE(*result_rv);
    EXPECT_EQ(result_rv->value(), 85);
}
#endif

}
