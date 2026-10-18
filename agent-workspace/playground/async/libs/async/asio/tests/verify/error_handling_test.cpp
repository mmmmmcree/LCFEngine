#include "../common/Runtime.h"
#include <asio/bind_cancellation_slot.hpp>
#include <asio/cancellation_signal.hpp>
#include <asio/steady_timer.hpp>
#include <gtest/gtest.h>
#include <chrono>
#include <algorithm>
#include <memory>
#include <string>

namespace {

namespace task = lcf::async::task;
using lcf::async::testing::Runtime;
using namespace std::chrono_literals;

struct RetryCase
{
    const char * m_name_p;
    unsigned m_failures;
    unsigned m_limit;
    int m_delay_ms;
};

struct PropagationCase
{
    const char * m_name_p;
    std::error_code m_source_error;
    std::error_code m_published_error;
};

struct HandlerExceptionCase
{
    const char * m_name_p;
    bool m_task_throws;
    bool m_coroutine;
    bool m_system_error;
};

using ErrorRetries = testing::TestWithParam<RetryCase>;
using ErrorPropagation = testing::TestWithParam<PropagationCase>;
using ErrorExceptions = testing::TestWithParam<HandlerExceptionCase>;
using UpstreamFailures = testing::TestWithParam<bool>;
using RecoveryPayloads = testing::TestWithParam<bool>;

/* # Error handling tests */

/* ## Retry policy */

/*
 * ### Retry boundaries preserve node state and inputs
 *
 * - **Objective:** Node-local retries, attempt numbering, stable inputs, and retry-delay boundaries.
 * - **Input:** A move-only seed `7`, stateful task and handler, configured failures and attempt limit; negative, zero, or positive retry delay.
 * - **Procedure:** Run a prefix once; retry the node until success or propagation; inspect each input address, callable counter, attempt count, and positive delay.
 * - **Expected:** Success value `15` with one successor call, or the original failure with no successor; the prefix runs once; successful first attempts bypass the handler.
 * - **Failure hints:** Check `invoke_task_with_error_handler()` in `error_handling.h`; named cases isolate retry limits and delay boundaries, while address and counter assertions isolate state preservation.
 */
void retries_preserve_node_state_and_inputs(const RetryCase & scenario);
TEST_P(ErrorRetries, PreservesNodeStateAndInputs)
{
    retries_preserve_node_state_and_inputs(GetParam());
}

/*
 * ### Cancellation interrupts a retry delay without another handler call
 *
 * - **Objective:** Cancellation of a pending retry timer and suppression of further attempts.
 * - **Input:** A failing node; a handler requesting a one-hour delay and posting terminal
 *   cancellation; completion bound to the cancellation slot.
 * - **Procedure:** Start the chain and run its context for up to one second.
 * - **Expected:** Completion is observed with error `operation_aborted`; attempt count `1` and
 *   handler count `1`.
 * - **Failure hints:** Check the retry timer in `invoke_task_with_error_handler()` and the cancellation slot from `Chain::start()`; attempt and handler counts detect an unwanted retry.
 */
void cancellation_ends_the_retry_delay_without_calling_the_handler_again();
TEST(ErrorHandling, CancellationEndsTheRetryDelayWithoutCallingTheHandlerAgain)
{
    cancellation_ends_the_retry_delay_without_calling_the_handler_again();
}

/* ## Recovery values */

/*
 * ### A move-only fallback becomes the published node result
 *
 * - **Objective:** Move-only recovery values and identity through a retained result token.
 * - **Input:** A node failing with `no_such_file_or_directory`; a handler owning a fallback
 *   `unique_ptr` containing `42`; a successor receiving both current and retained results.
 * - **Procedure:** Recover by moving the fallback into `use_value()`; execute the successor.
 * - **Expected:** Successful result with value `42`; both successor arguments refer to the same
 *   recovered value object.
 * - **Failure hints:** Check `use_value()` and final action extraction in `invoke_task_with_error_handler()`; unequal addresses indicate the recovered result was copied or republished separately.
 */
void move_only_fallback_becomes_the_nodes_published_result();
TEST(ErrorHandling, MoveOnlyFallbackBecomesTheNodesPublishedResult)
{
    move_only_fallback_becomes_the_nodes_published_result();
}

/*
 * ### Recovery supports void results and error codes as successful payloads
 *
 * - **Objective:** Preservation of the distinction between recovery status and payload type.
 * - **Input:** A failed void node recovered with `use_value()`; a failed `Result<error_code>` node
 *   recovered with payload `invalid_argument`.
 * - **Procedure:** Execute the selected recovery form; follow void recovery with a task returning `8`.
 * - **Expected:** The void case produces `8`; the error-code case succeeds with payload `invalid_argument`.
 * - **Failure hints:** Use the named payload form to check recovery action conversion in `invoke_task_with_error_handler()`; a nonzero error-code payload must remain a successful value.
 */
void recovery_preserves_the_payload_form(bool error_code_payload);
TEST_P(RecoveryPayloads, PreservesThePayloadForm)
{
    recovery_preserves_the_payload_form(GetParam());
}

/* ## Failure propagation */

/*
 * ### Upstream failure bypasses local task execution and recovery
 *
 * - **Objective:** Error handlers are invoked only for failures returned by their own node.
 * - **Input:** An upstream `invalid_argument` failure; either a chain successor or a junction link with a recovery handler.
 * - **Procedure:** Build the dependent node, start its source when needed, and execute it.
 * - **Expected:** The original upstream failure is preserved; neither the dependent task nor its handler runs.
 * - **Failure hints:** Use the dependency-kind name to inspect the failed-input branch in `Chain::link()` or `ChainJunction::link()` before `ErrorHandledTask` is invoked.
 */
void upstream_failure_skips_local_recovery(bool junction);
TEST_P(UpstreamFailures, SkipsTaskAndHandler)
{
    upstream_failure_skips_local_recovery(GetParam());
}

/*
 * ### Propagation selects an error independently of its boolean value
 *
 * - **Objective:** Preservation of explicit failure status during original-error propagation and replacement.
 * - **Input:** A node failing with a nonzero or zero error; a handler propagating the original or a replacement, including a zero-code replacement.
 * - **Procedure:** Execute the node and a success-only successor; inspect the handler context and final failure.
 * - **Expected:** One handler call at attempt `1`; exact selected error in a failed result; no successor call.
 * - **Failure hints:** Check `propagate()` and final `Result` extraction in `invoke_task_with_error_handler()`; zero-code cases distinguish explicit failure from error-code truthiness.
 */
void propagates_the_selected_error(const PropagationCase & scenario);
TEST_P(ErrorPropagation, PreservesExplicitFailureStatus)
{
    propagates_the_selected_error(GetParam());
}

/* ## Scheduling */

/*
 * ### Coroutine tasks and handlers resume on the node executor
 *
 * - **Objective:** Executor routing for asynchronous tasks, handlers, and retries.
 * - **Input:** A CPU seed `3`; a control-executor coroutine task that fails; a coroutine handler
 *   waiting `1ms`, retrying once, then recovering with `9`.
 * - **Procedure:** Run the chain while pumping the control context on the calling thread.
 * - **Expected:** Successful result with value `9`; attempt count `2` and handler count `2`; task
 *   and handler execute on the calling thread.
 * - **Failure hints:** Check the executor selected by `Chain::link()` and coroutine resumption through `invoke_task_with_error_handler()`; thread assertions identify task versus handler routing.
 */
void async_task_and_handler_use_the_nodes_executor();
TEST(ErrorHandling, AsyncTaskAndHandlerUseTheNodesExecutor)
{
    async_task_and_handler_use_the_nodes_executor();
}

/* ## Exception boundaries */

/*
 * ### Task and handler exceptions publish failures without recursive recovery
 *
 * - **Objective:** Exception conversion and token publication at the task and handler boundaries.
 * - **Input:** Either the task or handler throws; synchronous or suspended execution; system error `invalid_argument` or unknown exception.
 * - **Procedure:** Run the failing node; observe its retained token through a junction; check handler and successor invocation counts.
 * - **Expected:** Node and observer have the expected `invalid_argument` or `io_error` failure; task exceptions call no handler; handler exceptions call it once; no successor runs.
 * - **Failure hints:** Use the task/handler, sync/coroutine, and exception-category name to inspect `invoke_task_with_error_handler()` and outer `Chain::execute()` exception publication.
 */
void exceptions_bypass_recursive_recovery(const HandlerExceptionCase & scenario);
TEST_P(ErrorExceptions, PublishesFailureWithoutRecursiveHandling)
{
    exceptions_bypass_recursive_recovery(GetParam());
}

/* ## Junction recovery */

/*
 * ### Branches observe the final recovered source result and recover independently
 *
 * - **Objective:** Publication after retry completion and recovery on junction links.
 * - **Input:** A source failing twice before returning `5`; a left branch adding `1`; a right
 *   branch failing with `invalid_argument` and recovering with `7`.
 * - **Procedure:** Start the source and both branches; run a junction successor that sums their
 *   final values.
 * - **Expected:** Successful result with value `13`; source attempt count `3`, source handler count
 *   `2`, right-branch handler count `1`.
 * - **Failure hints:** Check notification timing in `execute_and_notify()` relative to retry completion; `branch_handled` isolates recovery on the junction link.
 */
void branches_observe_only_the_final_attempt_and_links_can_recover();
TEST(ErrorHandling, BranchesObserveOnlyTheFinalAttemptAndLinksCanRecover)
{
    branches_observe_only_the_final_attempt_and_links_can_recover();
}

/* ## Test support */

template <typename Case>
std::string case_name(const testing::TestParamInfo<Case> & info) { return info.param.m_name_p; }

void PrintTo(const RetryCase & scenario, std::ostream * output_p) { *output_p << scenario.m_name_p; }
void PrintTo(const PropagationCase & scenario, std::ostream * output_p) { *output_p << scenario.m_name_p; }
void PrintTo(const HandlerExceptionCase & scenario, std::ostream * output_p) { *output_p << scenario.m_name_p; }

INSTANTIATE_TEST_SUITE_P(RetryBoundaries, ErrorRetries, testing::Values(
    RetryCase {"ImmediateSuccess", 0u, 3u, 0},
    RetryCase {"RetryOnce", 1u, 3u, 0},
    RetryCase {"RetryTwice", 2u, 3u, 0},
    RetryCase {"NegativeDelay", 1u, 3u, -1},
    RetryCase {"PositiveDelay", 1u, 3u, 2},
    RetryCase {"PropagateFirstFailure", 1u, 1u, 0},
    RetryCase {"ExhaustedRetries", 3u, 3u, 0}), case_name<RetryCase>);

INSTANTIATE_TEST_SUITE_P(ErrorCodes, ErrorPropagation, testing::Values(
    PropagationCase {"OriginalError", std::make_error_code(std::errc::invalid_argument), std::make_error_code(std::errc::invalid_argument)},
    PropagationCase {"OriginalZeroCode", {}, {}},
    PropagationCase {"ReplacementError", std::make_error_code(std::errc::invalid_argument), std::make_error_code(std::errc::io_error)},
    PropagationCase {"ReplacementZeroCode", std::make_error_code(std::errc::invalid_argument), {}}), case_name<PropagationCase>);

INSTANTIATE_TEST_SUITE_P(ExceptionBoundaries, ErrorExceptions, testing::Values(
    HandlerExceptionCase {"TaskSyncSystemError", true, false, true},
    HandlerExceptionCase {"TaskSyncUnknown", true, false, false},
    HandlerExceptionCase {"TaskCoroutineSystemError", true, true, true},
    HandlerExceptionCase {"TaskCoroutineUnknown", true, true, false},
    HandlerExceptionCase {"HandlerSyncSystemError", false, false, true},
    HandlerExceptionCase {"HandlerSyncUnknown", false, false, false},
    HandlerExceptionCase {"HandlerCoroutineSystemError", false, true, true},
    HandlerExceptionCase {"HandlerCoroutineUnknown", false, true, false}), case_name<HandlerExceptionCase>);

INSTANTIATE_TEST_SUITE_P(DependencyKinds, UpstreamFailures, testing::Bool(),
    [](const testing::TestParamInfo<bool> & info) { return info.param ? "Junction" : "Chain"; });

INSTANTIATE_TEST_SUITE_P(PayloadForms, RecoveryPayloads, testing::Bool(),
    [](const testing::TestParamInfo<bool> & info) { return info.param ? "ErrorCode" : "Void"; });

/* ## Test implementations */

/* ### Retry policy */

void retries_preserve_node_state_and_inputs(const RetryCase & scenario)
{
    Runtime runtime;
    const auto failure = std::make_error_code(std::errc::resource_unavailable_try_again);
    const auto delay = std::chrono::milliseconds {scenario.m_delay_ms};
    unsigned prefix_runs = 0;
    unsigned attempts = 0;
    unsigned handled = 0;
    unsigned successor_runs = 0;
    const int * input_p = nullptr;
    std::chrono::steady_clock::time_point last_failure;
    auto seed = task::Chain {std::make_unique<int>(7), runtime.cpu()};
    auto original_token = seed.getResultToken();
    auto prefix = std::move(seed).link([&](const std::unique_ptr<int> & value_up) { ++prefix_runs; return *value_up; });
    auto current = std::move(prefix).link(
        [&, state_up = std::make_unique<unsigned>(0)](const int & value, const std::unique_ptr<int> & original_up) -> task::Result<int> {
            ++attempts;
            EXPECT_EQ(++*state_up, attempts);
            if (input_p) { EXPECT_EQ(input_p, &value); }
            input_p = &value;
            EXPECT_EQ(value, *original_up);
            if (attempts > 1u and delay > 0ms) { EXPECT_GE(std::chrono::steady_clock::now() - last_failure, delay); }
            if (attempts <= scenario.m_failures) { return std::unexpected(failure); }
            return value + *original_up;
        }, task::on_error([&, state_up = std::make_unique<unsigned>(0)](const task::ErrorHandleContext & context) -> task::ErrorAction<int> {
            ++handled;
            EXPECT_EQ(context.getErrorCode(), failure);
            EXPECT_EQ(context.getAttemptCount(), ++*state_up);
            EXPECT_EQ(context.getAttemptCount(), attempts);
            if (context.getAttemptCount() >= scenario.m_limit) { return task::propagate(context.getErrorCode()); }
            last_failure = std::chrono::steady_clock::now();
            return task::retry_after(delay);
        }), original_token);
    auto recovered_token = current.getResultToken();
    auto tail = std::move(current).link([&](const int & value, const int & repeated) {
        EXPECT_EQ(&value, &repeated);
        ++successor_runs;
        return value + 1;
    }, recovered_token);
    auto result_rh = runtime.run(std::move(tail));
    EXPECT_EQ(prefix_runs, 1u);
    EXPECT_EQ(attempts, std::min(scenario.m_failures + 1u, scenario.m_limit));
    EXPECT_EQ(handled, std::min(scenario.m_failures, scenario.m_limit));
    if (scenario.m_failures >= scenario.m_limit) {
        ASSERT_FALSE(*result_rh);
        EXPECT_EQ(result_rh->error(), failure);
        EXPECT_EQ(successor_runs, 0u);
        return;
    }
    ASSERT_TRUE(*result_rh);
    EXPECT_EQ(result_rh->value(), 15);
    EXPECT_EQ(successor_runs, 1u);
}

void cancellation_ends_the_retry_delay_without_calling_the_handler_again()
{
    asio::io_context context;
    asio::cancellation_signal cancellation;
    unsigned attempts = 0;
    unsigned handled = 0;
    bool completed = false;
    std::optional<std::error_code> failure_opt;
    auto chain = task::Chain {context.get_executor()}.link([&]() -> task::Result<int> {
        ++attempts;
        return std::unexpected(std::make_error_code(std::errc::resource_unavailable_try_again));
    }, task::on_error([&](const task::ErrorHandleContext &) {
        ++handled;
        asio::post(context, [&] { cancellation.emit(asio::cancellation_type::terminal); });
        return task::retry_after(1h);
    }));
    std::move(chain).start(context.get_executor(), asio::bind_cancellation_slot(cancellation.slot(),
        [&](const task::Result<int> & result) {
            if (not result) { failure_opt = result.error(); }
            completed = true;
        }));
    context.run_for(1s);
    ASSERT_TRUE(completed);
    ASSERT_TRUE(failure_opt);
    EXPECT_EQ(*failure_opt, asio::error::operation_aborted);
    EXPECT_EQ(attempts, 1u);
    EXPECT_EQ(handled, 1u);
}

/* ### Recovery values */

void move_only_fallback_becomes_the_nodes_published_result()
{
    Runtime runtime;
    auto current = task::Chain {runtime.cpu()}.link([]() -> task::Result<std::unique_ptr<int>> {
        return std::unexpected(std::make_error_code(std::errc::no_such_file_or_directory));
    }, task::on_error([fallback_up = std::make_unique<int>(42)](const task::ErrorHandleContext &) mutable {
        return task::use_value(std::move(fallback_up));
    }));
    auto recovered_token = current.getResultToken();
    auto tail = std::move(current).link(
        [](const std::unique_ptr<int> & value_up, const std::unique_ptr<int> & repeated_up) {
            EXPECT_EQ(&value_up, &repeated_up);
            return *value_up;
        }, recovered_token);
    auto result_rh = runtime.run(std::move(tail));
    ASSERT_TRUE(*result_rh);
    EXPECT_EQ(result_rh->value(), 42);
}

void recovery_preserves_the_payload_form(bool error_code_payload)
{
    Runtime runtime;
    const auto failure = std::make_error_code(std::errc::invalid_argument);
    if (error_code_payload) {
        auto chain = task::Chain {runtime.cpu()}.link([&]() -> task::Result<std::error_code> {
            return std::unexpected(failure);
        }, task::on_error([&](const task::ErrorHandleContext &) { return task::use_value(failure); }));
        auto result_rh = runtime.run(std::move(chain));
        ASSERT_TRUE(*result_rh);
        EXPECT_EQ(result_rh->value(), failure);
        return;
    }
    auto empty = task::Chain {runtime.cpu()}
        .link([&] { return failure; }, task::on_error([](const task::ErrorHandleContext &) { return task::use_value(); }))
        .link([] { return 8; });
    auto result_rh = runtime.run(std::move(empty));
    ASSERT_TRUE(*result_rh);
    EXPECT_EQ(result_rh->value(), 8);
}

/* ### Failure propagation */

void upstream_failure_skips_local_recovery(bool junction)
{
    Runtime runtime;
    const auto failure = std::make_error_code(std::errc::invalid_argument);
    unsigned task_runs = 0;
    unsigned handled = 0;
    auto source = task::Chain {runtime.cpu()}.link([&]() -> task::Result<int> { return std::unexpected(failure); });
    auto function = [&](const int &) { ++task_runs; return 0; };
    auto handler = [&](const task::ErrorHandleContext &) { ++handled; return task::use_value(42); };
    auto make_dependent = [&] {
        if (junction) { return task::merge(source.getResultToken()).link(runtime.cpu(), function, task::on_error(handler)); }
        return std::move(source).link(function, task::on_error(handler));
    };
    auto dependent = make_dependent();
    if (junction) { runtime.start(std::move(source)); }
    auto result_rh = runtime.run(std::move(dependent));
    ASSERT_FALSE(*result_rh);
    EXPECT_EQ(result_rh->error(), failure);
    EXPECT_EQ(task_runs, 0u);
    EXPECT_EQ(handled, 0u);
}

void propagates_the_selected_error(const PropagationCase & scenario)
{
    Runtime runtime;
    unsigned handled = 0;
    unsigned successor_runs = 0;
    auto chain = task::Chain {runtime.cpu()}
        .link([&]() -> task::Result<int> { return std::unexpected(scenario.m_source_error); },
            task::on_error([&](const task::ErrorHandleContext & context) {
                ++handled;
                EXPECT_EQ(context.getAttemptCount(), 1u);
                EXPECT_EQ(context.getErrorCode(), scenario.m_source_error);
                return task::propagate(scenario.m_published_error);
            }))
        .link([&](const int &) { ++successor_runs; });
    auto result_rh = runtime.run(std::move(chain));
    ASSERT_FALSE(*result_rh);
    EXPECT_EQ(result_rh->error(), scenario.m_published_error);
    EXPECT_EQ(handled, 1u);
    EXPECT_EQ(successor_runs, 0u);
}

/* ### Scheduling */

void async_task_and_handler_use_the_nodes_executor()
{
    Runtime runtime;
    const auto control_thread = std::this_thread::get_id();
    unsigned attempts = 0;
    unsigned handled = 0;
    auto chain = task::Chain {3, runtime.cpu()}.link(runtime.control(),
        [&](const int &) -> asio::awaitable<task::Result<int>> {
            EXPECT_EQ(std::this_thread::get_id(), control_thread);
            co_await asio::post(asio::use_awaitable);
            ++attempts;
            co_return std::unexpected(std::make_error_code(std::errc::resource_unavailable_try_again));
        }, task::on_error([&](const task::ErrorHandleContext & context) -> asio::awaitable<task::ErrorAction<int>> {
            EXPECT_EQ(std::this_thread::get_id(), control_thread);
            ++handled;
            asio::steady_timer timer {co_await asio::this_coro::executor};
            timer.expires_after(1ms);
            co_await timer.async_wait(asio::use_awaitable);
            if (context.getAttemptCount() < 2) { co_return task::retry_after(0ms); }
            co_return task::use_value(9);
        }));
    auto result_rh = runtime.run(std::move(chain));
    ASSERT_TRUE(*result_rh);
    EXPECT_EQ(result_rh->value(), 9);
    EXPECT_EQ(attempts, 2u);
    EXPECT_EQ(handled, 2u);
}

/* ### Exception boundaries */

void exceptions_bypass_recursive_recovery(const HandlerExceptionCase & scenario)
{
    Runtime runtime;
    const auto source_error = std::make_error_code(std::errc::resource_unavailable_try_again);
    const auto expected_error = std::make_error_code(scenario.m_system_error ? std::errc::invalid_argument : std::errc::io_error);
    auto raise = [&]() -> int {
        if (scenario.m_system_error) { throw std::system_error(expected_error); }
        throw std::runtime_error("exception");
    };
    unsigned handled = 0;
    unsigned successor_runs = 0;
    auto function = [&]() -> task::Result<int> {
        if (scenario.m_task_throws) { raise(); }
        return std::unexpected(source_error);
    };
    auto handler = [&](const task::ErrorHandleContext & context) -> task::ErrorAction<int> {
        ++handled;
        EXPECT_EQ(context.getErrorCode(), source_error);
        EXPECT_EQ(context.getAttemptCount(), 1u);
        return task::use_value(raise());
    };
    auto make_chain = [&] {
        if (scenario.m_coroutine) {
            return task::Chain {runtime.cpu()}.link([&]() -> asio::awaitable<task::Result<int>> {
                co_await asio::post(asio::use_awaitable);
                co_return function();
            }, task::on_error([&](const task::ErrorHandleContext & context) -> asio::awaitable<task::ErrorAction<int>> {
                co_await asio::post(asio::use_awaitable);
                co_return handler(context);
            }));
        }
        return task::Chain {runtime.cpu()}.link(function, task::on_error(handler));
    };
    auto chain = make_chain();
    auto token = chain.getResultToken();
    auto result_rh = runtime.run(std::move(chain));
    ASSERT_FALSE(*result_rh);
    EXPECT_EQ(result_rh->error(), expected_error);
    EXPECT_EQ(handled, scenario.m_task_throws ? 0u : 1u);
    auto observed = task::merge(token).link(runtime.cpu(), [&](const int &) { ++successor_runs; });
    auto observed_rh = runtime.run(std::move(observed));
    ASSERT_FALSE(*observed_rh);
    EXPECT_EQ(observed_rh->error(), expected_error);
    EXPECT_EQ(handled, scenario.m_task_throws ? 0u : 1u);
    EXPECT_EQ(successor_runs, 0u);
}

/* ### Junction recovery */

void branches_observe_only_the_final_attempt_and_links_can_recover()
{
    Runtime runtime;
    unsigned attempts = 0;
    unsigned handled = 0;
    unsigned branch_handled = 0;
    auto source = task::Chain {runtime.cpu()}.link([&]() -> task::Result<int> {
        if (++attempts < 3) { return std::unexpected(std::make_error_code(std::errc::resource_unavailable_try_again)); }
        return 5;
    }, task::on_error([&](const task::ErrorHandleContext &) { ++handled; return task::retry_after(0ms); }));
    auto junction = task::merge(source.getResultToken());
    auto left = junction.link(runtime.cpu(), [](const int & value) { return value + 1; });
    auto right = junction.link(runtime.cpu(), [](const int &) -> task::Result<int> {
        return std::unexpected(std::make_error_code(std::errc::invalid_argument));
    }, task::on_error([&](const task::ErrorHandleContext &) { ++branch_handled; return task::use_value(7); }));
    auto joined = task::merge(left.getResultToken(), right.getResultToken())
        .link(runtime.cpu(), [](const int & a, const int & b) { return a + b; });
    runtime.start(std::move(source));
    runtime.start(std::move(left));
    runtime.start(std::move(right));
    auto result_rh = runtime.run(std::move(joined));
    ASSERT_TRUE(*result_rh);
    EXPECT_EQ(result_rh->value(), 13);
    EXPECT_EQ(attempts, 3u);
    EXPECT_EQ(handled, 2u);
    EXPECT_EQ(branch_handled, 1u);
}

}
