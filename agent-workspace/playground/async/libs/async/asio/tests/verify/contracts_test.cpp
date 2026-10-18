#include "../common/Runtime.h"
#include <gtest/gtest.h>
#include <atomic>
#include <memory>
#include <memory_resource>
#include <optional>
#include <string>

#if LCF_ASYNC_USE_MODULES
namespace lcf::async::testing {

task::Chain<int> make_header_chain(asio::any_io_executor executor);
task::ChainJunction<int> describe_header_chain(const task::Chain<int> & chain);

}
#endif

namespace {

namespace task = lcf::async::task;
using lcf::async::testing::Runtime;
using IntegerToken = task::Chain<int>::ResultToken;
using VoidToken = task::Chain<void>::ResultToken;

using ResultStateLifetime = testing::TestWithParam<bool>;

/* # Contract tests */

/* ## Invalid usage */

#ifndef NDEBUG

/*
 * ### A chain rejects an unfinished independent token
 *
 * - **Objective:** Debug enforcement of readiness for additional chain tokens.
 * - **Input:** An unstarted independent source with seed `3`; another chain with seed `4` and the
 *   source token as an additional argument.
 * - **Procedure:** Start the dependent chain and run the context in a death-test subprocess.
 * - **Expected:** Process termination with the diagnostic that a result referenced by a
 *   `ResultToken` must already be completed, where death tests are supported.
 * - **Failure hints:** Check the additional-token readiness assertion in `Chain::link()`; the expected diagnostic distinguishes this contract from unrelated process termination.
 */
void chain_rejects_an_unready_independent_token();
TEST(ContractsDeathTest, ChainRejectsAnUnreadyIndependentToken)
{
    chain_rejects_an_unready_independent_token();
}

/*
 * ### A consumed chain rejects a second execution
 *
 * - **Objective:** Debug enforcement of single execution after `start()` consumes chain state.
 * - **Input:** A chain with seed `3` and one context executor.
 * - **Procedure:** Call `start()` twice on the same moved chain in a death-test subprocess.
 * - **Expected:** Process termination with the diagnostic that a consumed or moved-from chain
 *   cannot be executed, where death tests are supported.
 * - **Failure hints:** Check the consumed-state assertion in `Chain::execute()` and state transfer in `start()`.
 */
void chain_rejects_execution_after_start_consumes_its_state();
TEST(ContractsDeathTest, ChainRejectsExecutionAfterStartConsumesItsState)
{
    chain_rejects_execution_after_start_consumes_its_state();
}

/*
 * ### A junction successor rejects an unfinished additional token
 *
 * - **Objective:** Debug enforcement of readiness for tokens passed as additional link arguments.
 * - **Input:** A ready seed token `4`; an unstarted independent source with seed `3`; a junction
 *   link using the unfinished source as an additional token.
 * - **Procedure:** Start the junction successor and run the context in a death-test subprocess.
 * - **Expected:** Process termination with the diagnostic that an additional `ResultToken` must
 *   already be completed, where death tests are supported.
 * - **Failure hints:** Check the additional-token assertion in `ChainJunction::link()`; junction inputs and extra link arguments have different waiting contracts.
 */
void chain_junction_rejects_an_unready_additional_token();
TEST(ContractsDeathTest, ChainJunctionRejectsAnUnreadyAdditionalToken)
{
    chain_junction_rejects_an_unready_additional_token();
}

/*
 * ### Merge rejects a running source without completion notifications
 *
 * - **Objective:** Debug enforcement of junction construction before unfinished source execution.
 * - **Input:** A source coroutine suspended on a release channel; its retained result token.
 * - **Procedure:** Start and poll the source; construct a junction from its token in a death-test
 *   subprocess.
 * - **Expected:** Process termination with the diagnostic that `merge()` must be constructed before
 *   its unfinished source starts executing, where death tests are supported.
 * - **Failure hints:** Check the unconsumed-operation assertion in `ChainJunction::observe()` before installing completion notifications.
 */
void merge_rejects_attaching_notifications_to_a_running_source();
TEST(ContractsDeathTest, MergeRejectsAttachingNotificationsToARunningSource)
{
    merge_rejects_attaching_notifications_to_a_running_source();
}

#endif

/* ## Result ownership and lifetime */

/*
 * ### Result handles expose immutable values, void success, and failures
 *
 * - **Objective:** Immutable access to completed task results across supported result forms.
 * - **Input:** A seed `unique_ptr` containing `42`; a void seed; a task failing with
 *   `invalid_argument`.
 * - **Procedure:** Run each chain; verify the value handle's dereference type at compile time and
 *   inspect all results.
 * - **Expected:** Dereference yields a const result reference; move-only payload value `42`; void
 *   result succeeds; failed result retains `invalid_argument`.
 * - **Failure hints:** Check `ResultHandle` in `common.h` and the handle returned by `Chain::execute_result()`; compile-time assertions isolate const-access regressions.
 */
void result_handles_retain_immutable_task_results();
TEST(Contracts, ResultHandlesRetainImmutableTaskResults)
{
    result_handles_retain_immutable_task_results();
}

/*
 * ### Aliased result handles keep the owner alive without copying its result
 *
 * - **Objective:** Aliasing construction, shared ownership, and release on the final handle reset.
 * - **Input:** A shared owner containing a result with a `unique_ptr` payload `42`; a weak owner
 *   and a pointer to the embedded result.
 * - **Procedure:** Create a result handle by moving the owner; copy the handle; reset the original
 *   and then the copy.
 * - **Expected:** Construction is `noexcept`; the original owner pointer is empty; the handle
 *   aliases the embedded result with payload `42`; the owner expires only after the final reset.
 * - **Failure hints:** Check the aliasing shared-pointer constructor in `make_result_handle()`; pointer identity, payload, and weak-owner assertions isolate aliasing versus lifetime errors.
 */
void result_handles_retain_aliased_owners_without_copying_results();
TEST(Contracts, ResultHandlesRetainAliasedOwnersWithoutCopyingResults)
{
    result_handles_retain_aliased_owners_without_copying_results();
}

/*
 * ### Success and failure handles retain the original published state
 *
 * - **Objective:** Allocation-free result publication and state lifetime on both outcomes.
 * - **Input:** A tracking resource; a node publishing a move-only value `42` or failure `invalid_argument`; retained token and result handles.
 * - **Procedure:** Record allocations, execute the node, destroy its runtime, then release the token and handles one at a time.
 * - **Expected:** No execution-time state allocations; success payload or failure retained; state remains allocated until the last result handle is reset.
 * - **Failure hints:** Check `Chain::execute_result()` and `make_result_handle()`; allocation counts detect copied state, and reset checkpoints identify premature release. The failure case also checks `Runtime::run()` preserves the original result.
 */
void result_handles_retain_the_published_state(bool failure);
TEST_P(ResultStateLifetime, RetainsPublishedStateUntilLastOwner)
{
    result_handles_retain_the_published_state(GetParam());
}

/* ## Memory resources */

/*
 * ### A junction allocates its waiters from its own memory resource
 *
 * - **Objective:** Separation of source and junction allocations and balanced release.
 * - **Input:** Separate tracking resources for a source with seed `3` and increment task, and a
 *   junction successor with another increment task.
 * - **Procedure:** Start and poll the successor before starting the source; inspect allocation
 *   counts, then start the source and run to completion; destroy all state.
 * - **Expected:** The successor waits and adds junction-resource allocations only; result value
 *   `5`; source allocation count stays fixed; both resources release all allocations.
 * - **Failure hints:** Check `make_completion_state_handle()` and `make_completion_channel_handle()` in `ChainJunction.h`; separate source and junction counters identify the allocator that was bypassed.
 */
void chain_junction_owns_all_waiter_allocations();
TEST(Contracts, ChainJunctionOwnsAllWaiterAllocations)
{
    chain_junction_owns_all_waiter_allocations();
}

/*
 * ### Links inherit resources and explicit merges retain their chosen resource
 *
 * - **Objective:** Memory-resource propagation through links, merges, recovery, and retained
 *   results.
 * - **Input:** Three tracking resources; source values `3` and `4` plus a void source; a default
 *   junction and an explicit-resource junction; recovery value `14`.
 * - **Procedure:** Inspect resource selection on source links and junctions; start all sources and
 *   run a recovering junction successor; retain then reset its result handle.
 * - **Expected:** Links keep their source or junction resource; the default junction uses the
 *   default resource; result value `14`; source resources release all state before handle reset and
 *   the merge resource releases its remaining state afterward.
 * - **Failure hints:** Check resource forwarding in `Chain::link()`, `merge()`, and `ChainJunction::link()`; each pointer assertion identifies the incorrect resource selection, and reset counts isolate final-state lifetime.
 */
void memory_resources_propagate_across_links_and_merges();
TEST(Contracts, MemoryResourcesPropagateAcrossLinksAndMerges)
{
    memory_resources_propagate_across_links_and_merges();
}

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

/* ## Test support */

class TrackingResource final : public std::pmr::memory_resource
{
public:
    std::size_t getAllocationCount() const noexcept { return m_allocation_count.load(std::memory_order_relaxed); }
    std::size_t getDeallocationCount() const noexcept { return m_deallocation_count.load(std::memory_order_relaxed); }
private:
    void * do_allocate(std::size_t bytes, std::size_t alignment) override
    {
        auto * memory_p = std::pmr::new_delete_resource()->allocate(bytes, alignment);
        m_allocation_count.fetch_add(1, std::memory_order_relaxed);
        return memory_p;
    }
    void do_deallocate(void * memory_p, std::size_t bytes, std::size_t alignment) override
    {
        std::pmr::new_delete_resource()->deallocate(memory_p, bytes, alignment);
        m_deallocation_count.fetch_add(1, std::memory_order_relaxed);
    }
    bool do_is_equal(const std::pmr::memory_resource & other) const noexcept override { return this == &other; }
private:
    std::atomic_size_t m_allocation_count = 0;
    std::atomic_size_t m_deallocation_count = 0;
};

struct IntegerTask
{
    int operator()(const int & value) const { return value + 1; }
};

using NothrowIntegerTask = decltype([](const int & value) noexcept { return value + 1; });

struct DependencyTask
{
    int operator()(const int & a, const int & b) const { return a + b; }
};

struct WrongInputTask
{
    int operator()(std::string &) const { return 0; }
};

struct FakeToken
{
    using value_type = int;
};

struct RecoverInteger
{
    task::Result<int> operator()(const task::ErrorHandleContext &) const { return task::use_value(42); }
};

struct WrongRecoveryValue
{
    task::Result<std::string> operator()(const task::ErrorHandleContext &) const { return task::use_value(std::string {"wrong"}); }
};

struct MissingRecoveryDecision
{
    void operator()(const task::ErrorHandleContext &) const {}
};

struct NotAnErrorHandler {};

struct ThrowingMoveRecovery
{
    ThrowingMoveRecovery() noexcept = default;
    ThrowingMoveRecovery(ThrowingMoveRecovery &&) noexcept(false) {}
    task::Result<int> operator()(const task::ErrorHandleContext &) const noexcept { return task::use_value(42); }
};

template <typename Source, typename Function, typename... Tokens>
concept linkable_c = requires(Source && source, Function function, Tokens... tokens) {
    std::forward<Source>(source).link(std::move(function), tokens...);
};

template <typename Source, typename Function, typename... Tokens>
concept linkable_with_executor_c = requires(Source && source, asio::any_io_executor executor, Function function, Tokens... tokens) {
    std::forward<Source>(source).link(executor, std::move(function), tokens...);
};

template <typename... Sources>
concept mergeable_c = requires(Sources &&... sources) { task::merge(std::forward<Sources>(sources)...); };

template <typename... Sources>
concept mergeable_with_resource_c = requires(Sources &&... sources) {
    task::merge(std::pmr::get_default_resource(), std::forward<Sources>(sources)...);
};

template <typename Access>
concept result_readable_c = requires(const Access & access) { *access; access.operator->(); };

template <typename Source>
concept readiness_queryable_c = requires(const Source & source) { source.isReady(); };

template <typename Source>
concept result_queryable_c = requires(const Source & source) { source.getResult(); };

template <typename Source>
concept token_queryable_c = requires(const Source & source) { source.getResultToken(); };

template <typename Source>
concept runnable_c = requires(Source && source) { std::forward<Source>(source).run(); };

template <typename Source>
concept startable_c = requires(Source && source, asio::any_io_executor executor) {
    std::forward<Source>(source).start(executor, [](const auto &) {});
};

template <typename Source>
concept startable_without_executor_c = requires(Source && source) {
    std::forward<Source>(source).start([](const auto &) {});
};

template <typename Handler>
concept error_handler_wrapper_c = requires(Handler handler) { task::on_error(std::move(handler)); };

/* ## Compile-time contracts */

static_assert(linkable_c<task::Chain<int>, IntegerTask>);
static_assert(not linkable_c<task::Chain<int> &, IntegerTask>);
static_assert(not linkable_c<task::Chain<int>, WrongInputTask>);
static_assert(linkable_c<task::Chain<int>, DependencyTask, IntegerToken>);
static_assert(linkable_c<task::Chain<int>, IntegerTask, VoidToken>);
static_assert(not linkable_c<task::Chain<int>, DependencyTask, FakeToken>);
static_assert(not linkable_c<task::Chain<int>, DependencyTask, task::ResultHandle<int>>);
static_assert(linkable_c<task::Chain<int>, IntegerTask, task::ErrorHandler<RecoverInteger>>);
static_assert(linkable_c<task::Chain<int>, DependencyTask, task::ErrorHandler<RecoverInteger>, IntegerToken>);
static_assert(not linkable_c<task::Chain<int>, WrongInputTask, task::ErrorHandler<RecoverInteger>>);
static_assert(not linkable_c<task::Chain<int>, IntegerTask, task::ErrorHandler<WrongRecoveryValue>>);
static_assert(not linkable_c<task::Chain<int>, IntegerTask, task::ErrorHandler<MissingRecoveryDecision>>);
static_assert(linkable_with_executor_c<const task::ChainJunction<int> &, IntegerTask>);
static_assert(linkable_with_executor_c<const task::ChainJunction<int, void, int> &, DependencyTask>);
static_assert(linkable_with_executor_c<const task::ChainJunction<int> &, DependencyTask, IntegerToken>);
static_assert(linkable_with_executor_c<const task::ChainJunction<int> &, IntegerTask, task::ErrorHandler<RecoverInteger>>);
static_assert(linkable_with_executor_c<const task::ChainJunction<int> &, DependencyTask, task::ErrorHandler<RecoverInteger>, IntegerToken>);
static_assert(not linkable_c<const task::ChainJunction<int> &, IntegerTask>);
static_assert(not linkable_with_executor_c<const task::ChainJunction<int> &, WrongInputTask>);
static_assert(not linkable_with_executor_c<const task::ChainJunction<int> &, IntegerTask, task::ErrorHandler<WrongRecoveryValue>>);
static_assert(not linkable_with_executor_c<const task::ChainJunction<int> &, DependencyTask, FakeToken>);
static_assert(not linkable_with_executor_c<const task::ChainJunction<int> &, DependencyTask, task::ResultHandle<int>>);
static_assert(error_handler_wrapper_c<RecoverInteger>);
static_assert(not error_handler_wrapper_c<NotAnErrorHandler>);
static_assert(std::copyable<task::ResultHandle<int>>);
static_assert(result_readable_c<task::ResultHandle<int>>);
static_assert(result_readable_c<task::ResultHandle<void>>);
static_assert(std::copyable<IntegerToken>);
static_assert(std::copyable<VoidToken>);
static_assert(not result_readable_c<IntegerToken>);
static_assert(not readiness_queryable_c<IntegerToken>);
static_assert(not result_readable_c<VoidToken>);
static_assert(not readiness_queryable_c<VoidToken>);
static_assert(mergeable_c<IntegerToken>);
static_assert(mergeable_c<const IntegerToken &, VoidToken, IntegerToken &>);
static_assert(not mergeable_c<>);
static_assert(not mergeable_c<FakeToken>);
static_assert(not mergeable_c<int, IntegerToken>);
static_assert(not mergeable_c<task::Chain<int>, IntegerToken>);
static_assert(not mergeable_c<task::ChainJunction<int>, IntegerToken>);
static_assert(mergeable_with_resource_c<IntegerToken>);
static_assert(mergeable_with_resource_c<IntegerToken, VoidToken>);
static_assert(not mergeable_with_resource_c<>);
static_assert(not mergeable_with_resource_c<FakeToken>);
static_assert(not std::copy_constructible<task::Chain<int>>);
static_assert(std::copyable<task::ChainJunction<int>>);
static_assert(not std::constructible_from<task::ChainJunction<int>, int, asio::any_io_executor>);
static_assert(not std::constructible_from<task::ChainJunction<void>, asio::any_io_executor>);
static_assert(not readiness_queryable_c<task::ChainJunction<int>>);
static_assert(not result_queryable_c<task::ChainJunction<int>>);
static_assert(not token_queryable_c<task::ChainJunction<int>>);
static_assert(not runnable_c<task::Chain<int>>);
static_assert(not runnable_c<task::ChainJunction<int>>);
static_assert(startable_c<task::Chain<int>>);
static_assert(not startable_c<task::ChainJunction<int>>);
static_assert(not startable_without_executor_c<task::Chain<int>>);
static_assert(std::same_as<decltype(std::declval<const task::ChainJunction<int> &>().link(
    std::declval<asio::any_io_executor>(), IntegerTask {})), task::Chain<int>>);

/* ### Exception specifications */

static_assert(noexcept(task::on_error(RecoverInteger {})));
static_assert(not noexcept(task::on_error(ThrowingMoveRecovery {})));
static_assert(noexcept(task::use_value(42)));
static_assert(noexcept(task::use_value()));
static_assert(not noexcept(std::declval<task::Chain<int> &&>().link(NothrowIntegerTask {})));
static_assert(not noexcept(std::declval<task::Chain<int> &&>().start(
    std::declval<asio::any_io_executor>(), [](const task::Result<int> &) noexcept {})));

#if not LCF_ASYNC_USE_MODULES
using IntegerArguments = std::tuple<const int &>;
using NothrowVoidTask = decltype([]() noexcept {});
using NothrowStatusTask = decltype([]() noexcept { return std::error_code {}; });
using NothrowResultTask = decltype([]() noexcept { return task::Result<int> {42}; });
using NothrowRecoveryTask = decltype([]() noexcept { return ThrowingMoveRecovery {}; });

static_assert(noexcept(task::details::invoke_task_result(
    std::declval<NothrowIntegerTask &>(), std::declval<IntegerArguments &>())));
static_assert(not noexcept(task::details::invoke_task_result(
    std::declval<IntegerTask &>(), std::declval<IntegerArguments &>())));
static_assert(noexcept(task::details::invoke_task_result(
    std::declval<NothrowVoidTask &>(), std::declval<std::tuple<> &>())));
static_assert(noexcept(task::details::invoke_task_result(
    std::declval<NothrowStatusTask &>(), std::declval<std::tuple<> &>())));
static_assert(noexcept(task::details::invoke_task_result(
    std::declval<NothrowResultTask &>(), std::declval<std::tuple<> &>())));
static_assert(not noexcept(task::details::invoke_task_result(
    std::declval<NothrowRecoveryTask &>(), std::declval<std::tuple<> &>())));
static_assert(std::is_nothrow_constructible_v<
    task::details::ErrorHandledTask<NothrowIntegerTask, RecoverInteger>, NothrowIntegerTask, RecoverInteger>);
static_assert(not std::is_nothrow_constructible_v<
    task::details::ErrorHandledTask<NothrowIntegerTask, ThrowingMoveRecovery>, NothrowIntegerTask, ThrowingMoveRecovery>);
#endif

INSTANTIATE_TEST_SUITE_P(Outcomes, ResultStateLifetime, testing::Bool(),
    [](const testing::TestParamInfo<bool> & info) { return info.param ? "Failure" : "Success"; });

/* ## Test implementations */

/* ### Invalid usage */

#ifndef NDEBUG

void chain_rejects_an_unready_independent_token()
{
    EXPECT_DEATH_IF_SUPPORTED(([] {
        asio::io_context context;
        auto source = task::Chain {3, context.get_executor()}.link(IntegerTask {});
        auto chain = task::Chain {4, context.get_executor()}.link(DependencyTask {}, source.getResultToken());
        std::move(chain).start(context.get_executor(), [](const auto &) {});
        context.run();
    }()), "the result referenced by a ResultToken must already be completed");
}

void chain_rejects_execution_after_start_consumes_its_state()
{
    EXPECT_DEATH_IF_SUPPORTED(([] {
        asio::io_context context;
        auto chain = task::Chain {3, context.get_executor()};
        std::move(chain).start(context.get_executor(), [](const auto &) {});
        std::move(chain).start(context.get_executor(), [](const auto &) {});
        context.run();
    }()), "a consumed or moved-from Chain cannot be executed");
}

void chain_junction_rejects_an_unready_additional_token()
{
    EXPECT_DEATH_IF_SUPPORTED(([] {
        asio::io_context context;
        auto source = task::Chain {3, context.get_executor()}.link(IntegerTask {});
        auto ready = task::Chain {4, context.get_executor()};
        auto chain = task::merge(ready.getResultToken()).link(context.get_executor(), DependencyTask {}, source.getResultToken());
        std::move(chain).start(context.get_executor(), [](const auto &) {});
        context.run();
    }()), "the result referenced by an additional ResultToken must already be completed");
}

void merge_rejects_attaching_notifications_to_a_running_source()
{
    EXPECT_DEATH_IF_SUPPORTED(([] {
        asio::io_context context;
        asio::experimental::concurrent_channel<void(std::error_code)> release {context, 1u};
        auto source = task::Chain {context.get_executor()}.link([&]() -> asio::awaitable<int> {
            co_await release.async_receive(asio::use_awaitable);
            co_return 42;
        });
        auto token = source.getResultToken();
        std::move(source).start(context.get_executor(), [](const auto &) {});
        context.poll();
        [[maybe_unused]] auto junction = task::merge(token);
    }()), "merge\\(\\) must be constructed before its unfinished source starts executing");
}

#endif

/* ### Result ownership and lifetime */

void result_handles_retain_immutable_task_results()
{
    Runtime runtime;
    auto result_rh = runtime.run(task::Chain {std::make_unique<int>(42), runtime.cpu()});
    static_assert(std::same_as<decltype(*result_rh), const task::Result<std::unique_ptr<int>> &>);
    ASSERT_TRUE(*result_rh);
    EXPECT_EQ(*result_rh->value(), 42);
    auto empty_rh = runtime.run(task::Chain {runtime.cpu()});
    EXPECT_TRUE(*empty_rh);
    const auto failure = std::make_error_code(std::errc::invalid_argument);
    auto failed_rh = runtime.run(task::Chain {runtime.cpu()}.link([failure]() -> task::Result<int> { return std::unexpected(failure); }));
    ASSERT_FALSE(*failed_rh);
    EXPECT_EQ(failed_rh->error(), failure);
}

void result_handles_retain_aliased_owners_without_copying_results()
{
    struct Owner
    {
        task::Result<std::unique_ptr<int>> m_result {std::in_place, std::make_unique<int>(42)};
    };
    auto owner_sp = std::make_shared<Owner>();
    std::weak_ptr<Owner> owner_wp = owner_sp;
    const auto * result_p = &owner_sp->m_result;
    static_assert(noexcept(task::make_result_handle(owner_sp, result_p)));
    auto result_rh = task::make_result_handle(std::move(owner_sp), result_p);
    EXPECT_FALSE(owner_sp);
    EXPECT_EQ(result_rh.get(), result_p);
    EXPECT_EQ(*result_rh->value(), 42);
    auto copied_rh = result_rh;
    result_rh.reset();
    EXPECT_FALSE(owner_wp.expired());
    copied_rh.reset();
    EXPECT_TRUE(owner_wp.expired());
}

void result_handles_retain_the_published_state(bool failure)
{
    TrackingResource resource;
    task::ResultHandle<std::unique_ptr<int>> result_rh;
    std::optional<task::Chain<std::unique_ptr<int>>::ResultToken> token_opt;
    {
        Runtime runtime;
        auto chain = task::Chain {runtime.cpu(), &resource}.link([failure]() -> task::Result<std::unique_ptr<int>> {
            if (failure) { return std::unexpected(std::make_error_code(std::errc::invalid_argument)); }
            return std::make_unique<int>(42);
        });
        const auto allocations = resource.getAllocationCount();
        token_opt.emplace(chain.getResultToken());
        result_rh = runtime.run(std::move(chain));
        EXPECT_EQ(resource.getAllocationCount(), allocations);
    }
    ASSERT_TRUE(result_rh);
    if (failure) {
        ASSERT_FALSE(*result_rh);
        EXPECT_EQ(result_rh->error(), std::make_error_code(std::errc::invalid_argument));
    } else {
        ASSERT_TRUE(*result_rh);
        EXPECT_EQ(*result_rh->value(), 42);
    }
    const auto released_inputs = resource.getDeallocationCount();
    EXPECT_LT(released_inputs, resource.getAllocationCount());
    auto copied_rh = result_rh;
    EXPECT_EQ(copied_rh.get(), result_rh.get());
    result_rh.reset();
    token_opt.reset();
    EXPECT_EQ(resource.getDeallocationCount(), released_inputs);
    copied_rh.reset();
    EXPECT_EQ(resource.getDeallocationCount(), resource.getAllocationCount());
}

/* ### Memory resources */

void chain_junction_owns_all_waiter_allocations()
{
    TrackingResource source_resource;
    TrackingResource junction_resource;
    {
        asio::io_context context;
        auto source = task::Chain {3, context.get_executor(), &source_resource}.link(IntegerTask {});
        const auto source_allocations = source_resource.getAllocationCount();
        auto junction = task::merge(&junction_resource, source.getResultToken());
        auto branch = junction.link(context.get_executor(), IntegerTask {});
        const auto junction_allocations = junction_resource.getAllocationCount();
        bool completed = false;
        std::move(branch).start(context.get_executor(), [&](const task::Result<int> & result) {
            completed = true;
            ASSERT_TRUE(result);
            EXPECT_EQ(result.value(), 5);
        });
        context.poll();
        EXPECT_FALSE(completed);
        EXPECT_GT(junction_resource.getAllocationCount(), junction_allocations);
        EXPECT_EQ(source_resource.getAllocationCount(), source_allocations);
        std::move(source).start(context.get_executor(), [](const auto &) {});
        context.run();
        EXPECT_TRUE(completed);
        EXPECT_EQ(source_resource.getAllocationCount(), source_allocations);
    }
    EXPECT_EQ(source_resource.getDeallocationCount(), source_resource.getAllocationCount());
    EXPECT_EQ(junction_resource.getDeallocationCount(), junction_resource.getAllocationCount());
}

void memory_resources_propagate_across_links_and_merges()
{
    TrackingResource source_resource;
    TrackingResource other_resource;
    TrackingResource merge_resource;
    task::ResultHandle<int> result_rh;
    {
        Runtime runtime;
        auto seed = task::Chain {3, runtime.cpu(), &source_resource};
        auto token = seed.getResultToken();
        auto chain = std::move(seed).link([](const int & value, const int & original) { return value + original; }, token);
        EXPECT_EQ(chain.getMemoryResource(), &source_resource);
        auto side = task::Chain {4, runtime.cpu(), &other_resource};
        auto empty = task::Chain {runtime.cpu(), &other_resource};
        auto default_joined = task::merge(chain.getResultToken(), side.getResultToken());
        EXPECT_EQ(default_joined.getMemoryResource(), std::pmr::get_default_resource());
        auto joined = task::merge(&merge_resource, chain.getResultToken(), side.getResultToken(), empty.getResultToken());
        EXPECT_EQ(joined.getMemoryResource(), &merge_resource);
        auto tail = joined.link(runtime.cpu(), [](const int & a, const int & b) -> task::Result<int> {
            return std::unexpected(std::make_error_code(std::errc::invalid_argument));
        }, task::on_error([](const task::ErrorHandleContext &) { return task::use_value(14); }));
        EXPECT_EQ(tail.getMemoryResource(), &merge_resource);
        runtime.start(std::move(chain));
        runtime.start(std::move(side));
        runtime.start(std::move(empty));
        result_rh = runtime.run(std::move(tail));
    }
    ASSERT_TRUE(*result_rh);
    EXPECT_EQ(result_rh->value(), 14);
    EXPECT_EQ(source_resource.getDeallocationCount(), source_resource.getAllocationCount());
    EXPECT_EQ(other_resource.getDeallocationCount(), other_resource.getAllocationCount());
    EXPECT_LT(merge_resource.getDeallocationCount(), merge_resource.getAllocationCount());
    result_rh.reset();
    EXPECT_EQ(merge_resource.getDeallocationCount(), merge_resource.getAllocationCount());
}

/* ### Header and module compatibility */

#if LCF_ASYNC_USE_MODULES

void header_and_module_share_task_types()
{
    Runtime runtime;
    auto chain = lcf::async::testing::make_header_chain(runtime.cpu()).link(IntegerTask {});
    auto shared = lcf::async::testing::describe_header_chain(chain);
    auto branch = shared.link(runtime.cpu(), IntegerTask {});
    auto joined = task::merge(chain.getResultToken(), branch.getResultToken()).link(runtime.cpu(), DependencyTask {});
    runtime.start(std::move(chain));
    runtime.start(std::move(branch));
    auto result_rh = runtime.run(std::move(joined));
    ASSERT_TRUE(*result_rh);
    EXPECT_EQ(result_rh->value(), 85);
}

#endif

}
