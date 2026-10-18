#include "../common/Runtime.h"
#include <asio/bind_cancellation_slot.hpp>
#include <asio/cancellation_signal.hpp>
#include <asio/experimental/channel_error.hpp>
#include <asio/steady_timer.hpp>
#include <gtest/gtest.h>
#include <atomic>
#include <array>

namespace {

namespace task = lcf::async::task;
using lcf::async::testing::Runtime;
using namespace std::chrono_literals;

struct JoinFailureCase
{
    const char * m_name_p;
    unsigned m_failure_mask;
    bool m_second_first;
    bool m_zero_error;
};

using JunctionFailures = testing::TestWithParam<JoinFailureCase>;

/* # Chain junction tests */

/* ## Scheduling */

/*
 * ### Starting a successor waits for explicitly started sources
 *
 * - **Objective:** Separation of successor execution from source startup.
 * - **Input:** An unstarted source producing `42`; a junction successor; source and successor
 *   invocation counts `0`.
 * - **Procedure:** Start and poll the successor before starting the source; then start the source
 *   and run the context.
 * - **Expected:** Neither task runs and completion remains pending before source startup; both
 *   tasks then run once and completion receives `42`.
 * - **Failure hints:** Check `ChainJunction::wait()` and subscription setup; source counts detect accidental source startup, while a pending final completion indicates a lost notification.
 */
void starting_a_successor_waits_without_starting_its_sources();
TEST(ChainJunction, StartingASuccessorWaitsWithoutStartingItsSources)
{
    starting_a_successor_waits_without_starting_its_sources();
}

/*
 * ### Independent sources keep their executors and join on the selected executor
 *
 * - **Objective:** Concurrent source execution and explicit successor executor routing.
 * - **Input:** Two CPU sources with seeds `0` and `1`, each waiting for both tasks to start; a
 *   control-executor successor.
 * - **Procedure:** Start both sources independently and run the successor to sum their results.
 * - **Expected:** Successful result with value `3`; source tasks run on different threads; the
 *   successor runs on the calling thread.
 * - **Failure hints:** Check source executor retention in `Chain::link()` and the executor passed to `ChainJunction::link()`; source-thread equality also indicates serialized execution.
 */
void independently_started_sources_keep_their_task_executors();
TEST(ChainJunction, IndependentlyStartedSourcesKeepTheirTaskExecutors)
{
    independently_started_sources_keep_their_task_executors();
}

/* ## Shared completion */

/*
 * ### One source completion wakes branches on different executors
 *
 * - **Objective:** Shared completion notification across branch executors.
 * - **Input:** A source suspended on a release channel; branches on separate control and worker
 *   contexts.
 * - **Procedure:** Start and poll both branches; start and release the source; pump both contexts
 *   until completion or a one-second deadline.
 * - **Expected:** Both branches complete successfully with values `42` and `43`; completion count
 *   `2`.
 * - **Failure hints:** Check `CompletionState::subscribe()` and `complete()` in `ChainJunction.h`; completion counts identify a lost wakeup across branch executors.
 */
void multiple_branches_on_different_executors_share_one_completion_signal();
TEST(ChainJunction, MultipleBranchesOnDifferentExecutorsShareOneCompletionSignal)
{
    multiple_branches_on_different_executors_share_one_completion_signal();
}

/*
 * ### Concurrent branches share the same source result across startup orders
 *
 * - **Objective:** Single source execution and shared result identity for multiple waiters.
 * - **Input:** For each of `64` iterations, one source producing an array of `64` copies of the
 *   iteration value and `8` CPU branches.
 * - **Procedure:** Alternate starting the source before and after branch startup; wait for every
 *   branch and the source completion.
 * - **Expected:** Source invocation count `1` per iteration; no branch failures; every branch
 *   observes the source's exact result object.
 * - **Failure hints:** Use iteration and branch diagnostics to inspect `CompletionState` synchronization and `ExecutionState` publication; source counts detect repeated execution, pointers detect copied results.
 */
void multiple_branches_wait_for_one_source_without_repeating_it();
TEST(ChainJunction, MultipleBranchesWaitForOneSourceWithoutRepeatingIt)
{
    multiple_branches_wait_for_one_source_without_repeating_it();
}

/* ## Ownership and lifetime */

/*
 * ### Retained tokens preserve a move-only result for repeated branches
 *
 * - **Objective:** Result lifetime and identity after the source chain is consumed.
 * - **Input:** A source producing a `unique_ptr` containing `42`; its result token and completed
 *   result handle, released before branch execution.
 * - **Procedure:** Execute the source; create a junction from the retained token; run one branch
 *   comparing duplicate-token arguments and another reading the value.
 * - **Expected:** Duplicate arguments refer to the completed result handle's exact value object;
 *   the later branch returns `42`.
 * - **Failure hints:** Check `ResultToken` shared ownership and the token tuple captured by `ChainJunction::link()`; the original result handle is released before branches execute.
 */
void tokens_retain_the_exact_move_only_result_after_source_destruction();
TEST(ChainJunction, TokensRetainTheExactMoveOnlyResultAfterSourceDestruction)
{
    tokens_retain_the_exact_move_only_result_after_source_destruction();
}

/*
 * ### A temporary junction transfers dependencies and releases completed captures
 *
 * - **Objective:** Successor ownership after junction destruction and task-capture release after
 *   completion.
 * - **Input:** A source and a successor capturing shared value `42`; the successor is created from
 *   a temporary junction; a weak owner tracks the capture.
 * - **Procedure:** Release the external shared owner; start the source and run the successor.
 * - **Expected:** Successor result value `84`; the weak owner expires after completion.
 * - **Failure hints:** Check dependency ownership captured by `ChainJunction::link()` and operation release in `execute_and_notify()`.
 */
void temporary_descriptor_transfers_tokens_and_releases_task_captures();
TEST(ChainJunction, TemporaryDescriptorTransfersTokensAndReleasesTaskCaptures)
{
    temporary_descriptor_transfers_tokens_and_releases_task_captures();
}

/* ## Cancellation */

/*
 * ### Branch cancellation leaves the source and sibling branch active
 *
 * - **Objective:** Cancellation isolation and publication of a cancelled branch's failure to
 *   dependents.
 * - **Input:** A source suspended on a release channel; two waiting branches; a cancellation slot
 *   on the first branch; an observer of its token.
 * - **Procedure:** Start both branches and the source; cancel the first branch while the source
 *   waits; start the observer, release the source, and run the context.
 * - **Expected:** First-branch error `channel_cancelled`; source and sibling remain pending until
 *   release, then succeed with sibling value `43`; the observer fails and the source runs once.
 * - **Failure hints:** Check the cancellation slot in `Chain::start()` and cancellation handling in `ChainJunction::wait()`; source and sibling flags detect cancellation escaping the selected branch.
 */
void cancelling_one_branch_leaves_its_source_and_other_branches_running();
TEST(ChainJunction, CancellingOneBranchLeavesItsSourceAndOtherBranchesRunning)
{
    cancelling_one_branch_leaves_its_source_and_other_branches_running();
}

/* ## Arguments and return values */

/*
 * ### Duplicate and intermediate tokens keep their order while void inputs are omitted
 *
 * - **Objective:** Argument ordering through repeated tokens, void omission, and nested junctions.
 * - **Input:** Sources producing `5`, `void`, and `2`; an intermediate junction receiving the first
 *   token twice around the void token.
 * - **Procedure:** Compute `10 * a + b` in the intermediate junction and again when joining its
 *   result with the source value `2`; start all sources and the intermediate chain.
 * - **Expected:** Final result value `552`.
 * - **Failure hints:** Check the input-token tuple in `ChainJunction::link()` and `merge_results_to_arguments()` in `common.h`; decimal-place encoding exposes argument-order changes.
 */
void duplicate_void_and_intermediate_inputs_preserve_argument_order();
TEST(ChainJunction, DuplicateVoidAndIntermediateInputsPreserveArgumentOrder)
{
    duplicate_void_and_intermediate_inputs_preserve_argument_order();
}

/*
 * ### Already completed void sources require no startup and add no arguments
 *
 * - **Objective:** The all-ready junction path with only void inputs.
 * - **Input:** Two ready void seed tokens; a successor taking no arguments.
 * - **Procedure:** Create the junction without starting either source; execute its successor.
 * - **Expected:** A successful result with value `42`.
 * - **Failure hints:** Check the zero-pending-source path in the junction constructor and void omission in `merge_results_to_arguments()`.
 */
void ready_void_sources_add_no_arguments();
TEST(ChainJunction, ReadyVoidSourcesAddNoArguments)
{
    ready_void_sources_add_no_arguments();
}

/* ## Failure propagation */

/*
 * ### Completion order does not change draining or failure selection
 *
 * - **Objective:** Waiting for every source and choosing the first failed input by token order.
 * - **Input:** Two channel-gated sources; success, either failure, both failures, or explicit zero-code failure; either source released first.
 * - **Procedure:** Start sources and successor; release one source and verify the join is still pending; release the other and inspect the result.
 * - **Expected:** Exactly one completion after both releases; first failed input wins, including zero-code errors; the successor runs only when both inputs succeed.
 * - **Failure hints:** Early completion points to `CompletionState::complete()/wait()`; wrong selected errors point to the input scan in `ChainJunction::link()`. Named cases distinguish source outcomes and release order.
 */
void waits_for_both_and_selects_failure_by_input_order(const JoinFailureCase & scenario);
TEST_P(JunctionFailures, WaitsForBothAndSelectsByInputOrder)
{
    waits_for_both_and_selects_failure_by_input_order(GetParam());
}

/*
 * ### A source exception publishes failure through direct and downstream tokens
 *
 * - **Objective:** Exception conversion and failure publication to every dependent state.
 * - **Input:** A source throwing `system_error(invalid_argument)`; its direct token and a
 *   downstream token; a delayed sibling producing `2`.
 * - **Procedure:** Merge all three tokens; start the throwing chain's tail and the sibling; run the
 *   junction successor.
 * - **Expected:** Failed result with error `invalid_argument`; throwing-source invocation count
 *   `1`.
 * - **Failure hints:** Check `ChainJunction::execute_and_notify()` exception publication and notification after failure; the source count detects repeated evaluation of the same state.
 */
void source_exceptions_publish_failures_to_all_dependent_states();
TEST(ChainJunction, SourceExceptionsPublishFailuresToAllDependentStates)
{
    source_exceptions_publish_failures_to_all_dependent_states();
}

/* ## Test support */

asio::awaitable<int> delayed_value(int value)
{
    asio::steady_timer timer {co_await asio::this_coro::executor};
    timer.expires_after(1ms);
    co_await timer.async_wait(asio::use_awaitable);
    co_return value;
}

void PrintTo(const JoinFailureCase & scenario, std::ostream * output_p) { *output_p << scenario.m_name_p; }

INSTANTIATE_TEST_SUITE_P(SourceOutcomes, JunctionFailures, testing::Values(
    JoinFailureCase {"BothSucceedForward", 0u, false, false},
    JoinFailureCase {"BothSucceedReverse", 0u, true, false},
    JoinFailureCase {"FirstFailsForward", 1u, false, false},
    JoinFailureCase {"FirstFailsReverse", 1u, true, false},
    JoinFailureCase {"SecondFailsForward", 2u, false, false},
    JoinFailureCase {"SecondFailsReverse", 2u, true, false},
    JoinFailureCase {"BothFailForward", 3u, false, false},
    JoinFailureCase {"BothFailReverse", 3u, true, false},
    JoinFailureCase {"ZeroFirstFailure", 3u, true, true},
    JoinFailureCase {"ZeroSecondFailure", 2u, false, true}),
    [](const testing::TestParamInfo<JoinFailureCase> & info) { return info.param.m_name_p; });

/* ## Test implementations */

/* ### Scheduling */

void starting_a_successor_waits_without_starting_its_sources()
{
    asio::io_context context;
    unsigned source_runs = 0;
    unsigned successor_runs = 0;
    bool completed = false;
    auto source = task::Chain {context.get_executor()}.link([&] { ++source_runs; return 42; });
    auto junction = task::merge(source.getResultToken());
    auto successor = junction.link(context.get_executor(), [&](const int & value) { ++successor_runs; return value; });
    std::move(successor).start(context.get_executor(), [&](const task::Result<int> & result) {
        completed = true;
        ASSERT_TRUE(result);
        EXPECT_EQ(result.value(), 42);
    });
    context.poll();
    EXPECT_EQ(source_runs, 0u);
    EXPECT_EQ(successor_runs, 0u);
    EXPECT_FALSE(completed);
    std::move(source).start(context.get_executor(), [](const auto &) {});
    context.run();
    EXPECT_TRUE(completed);
    EXPECT_EQ(source_runs, 1u);
    EXPECT_EQ(successor_runs, 1u);
}

void independently_started_sources_keep_their_task_executors()
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
    const auto control_thread = std::this_thread::get_id();
    auto joined = task::merge(image.getResultToken(), shader.getResultToken())
        .link(runtime.control(), [&](const int & a, const int & b) {
            EXPECT_EQ(std::this_thread::get_id(), control_thread);
            return a + b;
        });
    runtime.start(std::move(image));
    runtime.start(std::move(shader));
    auto result_rh = runtime.run(std::move(joined));
    ASSERT_TRUE(*result_rh);
    EXPECT_EQ(result_rh->value(), 3);
    EXPECT_NE(threads[0], threads[1]);
}

/* ### Shared completion */

void multiple_branches_on_different_executors_share_one_completion_signal()
{
    asio::io_context control;
    asio::io_context worker;
    asio::experimental::concurrent_channel<void(std::error_code)> release {control, 1u};
    auto source = task::Chain {control.get_executor()}.link([&]() -> asio::awaitable<int> {
        co_await release.async_receive(asio::use_awaitable);
        co_return 42;
    });
    auto junction = task::merge(source.getResultToken());
    auto control_branch = junction.link(control.get_executor(), [](const int & value) { return value; });
    auto worker_branch = junction.link(worker.get_executor(), [](const int & value) { return value + 1; });
    unsigned completions = 0;
    std::move(control_branch).start(control.get_executor(), [&](const task::Result<int> & result) {
        ++completions;
        ASSERT_TRUE(result);
        EXPECT_EQ(result.value(), 42);
    });
    control.poll();
    std::move(worker_branch).start(worker.get_executor(), [&](const task::Result<int> & result) {
        ++completions;
        ASSERT_TRUE(result);
        EXPECT_EQ(result.value(), 43);
    });
    worker.poll();
    std::move(source).start(control.get_executor(), [](const auto &) {});
    ASSERT_TRUE(release.try_send(std::error_code {}));
    const auto deadline = std::chrono::steady_clock::now() + 1s;
    while (completions != 2u and std::chrono::steady_clock::now() < deadline) {
        control.restart();
        control.poll();
        worker.restart();
        worker.poll();
    }
    EXPECT_EQ(completions, 2u);
}

void multiple_branches_wait_for_one_source_without_repeating_it()
{
    using Values = std::array<unsigned, 64>;
    Runtime runtime;
    for (unsigned iteration = 0; iteration < 64; ++iteration) {
        SCOPED_TRACE(iteration);
        std::atomic_uint source_runs = 0;
        std::atomic_uint completions = 0;
        std::array<const Values *, 8> value_pointers {};
        const Values * source_value_p = nullptr;
        bool source_completed = false;
        auto source = task::Chain {iteration, runtime.cpu()}.link([&](const unsigned & value) {
            ++source_runs;
            Values values;
            values.fill(value);
            return values;
        });
        auto junction = task::merge(source.getResultToken());
        auto start_source = [&] {
            std::move(source).start(runtime.control(), [&](const task::Result<Values> & result) {
                source_completed = true;
                ASSERT_TRUE(result);
                source_value_p = &result.value();
            });
        };
        if (iteration % 2 == 0) { start_source(); }
        for (std::size_t index = 0; index < value_pointers.size(); ++index) {
            auto branch = junction.link(runtime.cpu(), [&, index](const Values & values) {
                value_pointers[index] = &values;
                Values expected;
                expected.fill(iteration);
                EXPECT_EQ(values, expected) << "iteration " << iteration << ", branch " << index;
            });
            std::move(branch).start(runtime.cpu(), [&, index](const task::Result<void> & result) {
                EXPECT_TRUE(result) << "iteration " << iteration << ", branch " << index;
                ++completions;
            });
        }
        if (iteration % 2 != 0) { start_source(); }
        runtime.pumpUntil([&] { return completions == value_pointers.size() and source_completed; });
        ASSERT_NE(source_value_p, nullptr);
        for (std::size_t index = 0; index < value_pointers.size(); ++index) {
            EXPECT_EQ(value_pointers[index], source_value_p) << "branch " << index;
        }
        EXPECT_EQ(source_runs, 1u);
    }
}

/* ### Ownership and lifetime */

void tokens_retain_the_exact_move_only_result_after_source_destruction()
{
    Runtime runtime;
    auto source = task::Chain {runtime.cpu()}.link([] { return std::make_unique<int>(42); });
    auto token = source.getResultToken();
    auto result_rh = runtime.run(std::move(source));
    const auto * original_value_p = &result_rh->value();
    result_rh.reset();
    auto junction = task::merge(token);
    auto checked = junction.link(runtime.cpu(), [](const std::unique_ptr<int> & value_up, const std::unique_ptr<int> & original_up) {
        return &value_up == &original_up ? &value_up : nullptr;
    }, token);
    auto checked_rh = runtime.run(std::move(checked));
    ASSERT_TRUE(*checked_rh);
    EXPECT_EQ(checked_rh->value(), original_value_p);
    auto repeated = junction.link(runtime.cpu(), [](const std::unique_ptr<int> & value_up) { return *value_up; });
    auto repeated_rh = runtime.run(std::move(repeated));
    ASSERT_TRUE(*repeated_rh);
    EXPECT_EQ(repeated_rh->value(), 42);
}

void temporary_descriptor_transfers_tokens_and_releases_task_captures()
{
    Runtime runtime;
    auto capture_sp = std::make_shared<int>(42);
    std::weak_ptr<int> capture_wp = capture_sp;
    auto source = task::Chain {runtime.cpu()}.link([capture_sp] { return *capture_sp; });
    auto successor = task::merge(source.getResultToken()).link(runtime.cpu(), [capture_sp](const int & value) {
        return value + *capture_sp;
    });
    capture_sp.reset();
    runtime.start(std::move(source));
    auto result_rh = runtime.run(std::move(successor));
    ASSERT_TRUE(*result_rh);
    EXPECT_EQ(result_rh->value(), 84);
    EXPECT_TRUE(capture_wp.expired());
}

/* ### Cancellation */

void cancelling_one_branch_leaves_its_source_and_other_branches_running()
{
    asio::io_context context;
    asio::cancellation_signal cancellation;
    asio::experimental::concurrent_channel<void(std::error_code)> release {context, 1u};
    unsigned source_runs = 0;
    bool cancelled = false;
    bool completed = false;
    bool source_completed = false;
    auto source = task::Chain {context.get_executor()}.link([&]() -> asio::awaitable<int> {
        ++source_runs;
        co_await release.async_receive(asio::use_awaitable);
        co_return 42;
    });
    auto junction = task::merge(source.getResultToken());
    auto first = junction.link(context.get_executor(), [](const int & value) { return value; });
    auto second = junction.link(context.get_executor(), [](const int & value) { return value + 1; });
    auto cancelled_token = first.getResultToken();
    auto observer = task::merge(cancelled_token).link(context.get_executor(), [](const int &) {});
    std::move(first).start(context.get_executor(), asio::bind_cancellation_slot(cancellation.slot(),
        [&](const task::Result<int> & result) {
            cancelled = true;
            ASSERT_FALSE(result);
            EXPECT_EQ(result.error(), asio::experimental::error::channel_cancelled);
        }));
    std::move(second).start(context.get_executor(), [&](const task::Result<int> & result) {
        completed = true;
        ASSERT_TRUE(result);
        EXPECT_EQ(result.value(), 43);
    });
    std::move(source).start(context.get_executor(), [&](const auto & result) {
        EXPECT_TRUE(result);
        source_completed = true;
    });
    context.poll();
    ASSERT_EQ(source_runs, 1u);
    ASSERT_FALSE(completed);
    cancellation.emit(asio::cancellation_type::terminal);
    context.poll();
    ASSERT_TRUE(cancelled);
    EXPECT_FALSE(completed);
    EXPECT_FALSE(source_completed);
    bool observed_cancellation = false;
    std::move(observer).start(context.get_executor(), [&](const auto & result) {
        EXPECT_FALSE(result);
        observed_cancellation = true;
    });
    ASSERT_TRUE(release.try_send(std::error_code {}));
    context.run();
    EXPECT_TRUE(completed);
    EXPECT_TRUE(source_completed);
    EXPECT_TRUE(observed_cancellation);
    EXPECT_EQ(source_runs, 1u);
}

/* ### Arguments and return values */

void duplicate_void_and_intermediate_inputs_preserve_argument_order()
{
    Runtime runtime;
    auto source = task::Chain {5, runtime.cpu()}.link([](const int & value) { return value; });
    auto empty = task::Chain {runtime.cpu()}.link([] {});
    auto other = task::Chain {2, runtime.cpu()}.link([](const int & value) { return value; });
    auto intermediate = task::merge(source.getResultToken(), empty.getResultToken(), source.getResultToken())
        .link(runtime.cpu(), [](const int & a, const int & b) { return 10 * a + b; });
    auto joined = task::merge(intermediate.getResultToken(), other.getResultToken())
        .link(runtime.cpu(), [](const int & a, const int & b) { return 10 * a + b; });
    runtime.start(std::move(source));
    runtime.start(std::move(empty));
    runtime.start(std::move(other));
    runtime.start(std::move(intermediate));
    auto result_rh = runtime.run(std::move(joined));
    ASSERT_TRUE(*result_rh);
    EXPECT_EQ(result_rh->value(), 552);
}

void ready_void_sources_add_no_arguments()
{
    Runtime runtime;
    auto first = task::Chain {runtime.cpu()};
    auto second = task::Chain {runtime.cpu()};
    auto joined = task::merge(first.getResultToken(), second.getResultToken()).link(runtime.cpu(), [] { return 42; });
    auto result_rh = runtime.run(std::move(joined));
    ASSERT_TRUE(*result_rh);
    EXPECT_EQ(result_rh->value(), 42);
}

/* ### Failure propagation */

void waits_for_both_and_selects_failure_by_input_order(const JoinFailureCase & scenario)
{
    asio::io_context context;
    asio::experimental::concurrent_channel<void(std::error_code)> first_release {context, 1u};
    asio::experimental::concurrent_channel<void(std::error_code)> second_release {context, 1u};
    std::array<unsigned, 2> source_runs {};
    const auto first_error = scenario.m_zero_error ? std::error_code {} : std::make_error_code(std::errc::invalid_argument);
    const auto second_error = scenario.m_zero_error and scenario.m_failure_mask == 2u
        ? std::error_code {} : std::make_error_code(std::errc::io_error);
    auto make_source = [&](unsigned index, auto & release) {
        return task::Chain {context.get_executor()}.link([&, index]() -> asio::awaitable<task::Result<int>> {
            co_await release.async_receive(asio::use_awaitable);
            ++source_runs[index];
            if (scenario.m_failure_mask & (1u << index)) { co_return std::unexpected(index == 0u ? first_error : second_error); }
            co_return static_cast<int>(index + 1u);
        });
    };
    auto first = make_source(0u, first_release);
    auto second = make_source(1u, second_release);
    unsigned successor_runs = 0;
    unsigned completions = 0;
    std::optional<task::Result<int>> output_opt;
    auto joined = task::merge(first.getResultToken(), second.getResultToken())
        .link(context.get_executor(), [&](const int & a, const int & b) { ++successor_runs; return a + b; });
    std::move(joined).start(context.get_executor(), [&](const task::Result<int> & result) { ++completions; output_opt = result; });
    std::move(first).start(context.get_executor(), [](const auto &) {});
    std::move(second).start(context.get_executor(), [](const auto &) {});
    context.poll();
    EXPECT_EQ(completions, 0u) << "join completed before either source was released";
    auto & earlier = scenario.m_second_first ? second_release : first_release;
    auto & later = scenario.m_second_first ? first_release : second_release;
    ASSERT_TRUE(earlier.try_send(std::error_code {}));
    context.restart();
    context.poll();
    EXPECT_EQ(source_runs[scenario.m_second_first ? 1u : 0u], 1u);
    EXPECT_EQ(source_runs[scenario.m_second_first ? 0u : 1u], 0u);
    EXPECT_EQ(completions, 0u) << "join completed while its remaining source was still suspended";
    ASSERT_TRUE(later.try_send(std::error_code {}));
    context.restart();
    context.run_for(1s);
    ASSERT_EQ(completions, 1u);
    ASSERT_TRUE(output_opt);
    EXPECT_EQ(source_runs, (std::array<unsigned, 2> {1u, 1u}));
    if (scenario.m_failure_mask != 0u) {
        ASSERT_FALSE(*output_opt);
        EXPECT_EQ(output_opt->error(), scenario.m_failure_mask & 1u ? first_error : second_error);
        EXPECT_EQ(successor_runs, 0u);
        return;
    }
    ASSERT_TRUE(*output_opt);
    EXPECT_EQ(output_opt->value(), 3);
    EXPECT_EQ(successor_runs, 1u);
}

void source_exceptions_publish_failures_to_all_dependent_states()
{
    Runtime runtime;
    const auto failure = std::make_error_code(std::errc::invalid_argument);
    std::atomic_uint source_runs = 0;
    auto throwing = task::Chain {runtime.cpu()}.link([&]() -> int {
        ++source_runs;
        throw std::system_error(failure);
    });
    auto throwing_token = throwing.getResultToken();
    auto tail = std::move(throwing).link([](const int & value) { return value; });
    auto sibling = task::Chain {2, runtime.cpu()}.link([](const int & value) { return delayed_value(value); });
    auto joined = task::merge(throwing_token, tail.getResultToken(), sibling.getResultToken())
        .link(runtime.cpu(), [](const int &, const int &, const int &) { return 0; });
    runtime.start(std::move(tail));
    runtime.start(std::move(sibling));
    auto result_rh = runtime.run(std::move(joined));
    ASSERT_FALSE(*result_rh);
    EXPECT_EQ(result_rh->error(), failure);
    EXPECT_EQ(source_runs, 1u);
}

}
