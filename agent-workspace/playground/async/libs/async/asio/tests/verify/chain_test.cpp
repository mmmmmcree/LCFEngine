#include "../common/Runtime.h"
#include <asio/steady_timer.hpp>
#include <gtest/gtest.h>
#include <atomic>
#include <array>
#include <string>

namespace {

namespace task = lcf::async::task;
using lcf::async::testing::Runtime;
using namespace std::chrono_literals;

struct ReturnCase
{
    const char * m_name_p;
    void (*m_verify_p)(Runtime &);
};

struct ExceptionCase
{
    const char * m_name_p;
    bool m_coroutine;
    bool m_system_error;
};

struct CaptureCase
{
    const char * m_name_p;
    unsigned m_outcome;
    bool m_coroutine;
};

using ChainReturns = testing::TestWithParam<ReturnCase>;
using ChainTaskExceptions = testing::TestWithParam<ExceptionCase>;
using ChainCaptureLifetime = testing::TestWithParam<CaptureCase>;

/* # Chain tests */

/* ## Scheduling */

/*
 * ### Work and completion follow their assigned executors
 *
 * - **Objective:** Explicit and inherited executor routing for nodes and completion delivery.
 * - **Input:** Seed value `3`; first node on the CPU executor; second node and
 *   completion on the control executor, pumped by the calling thread.
 * - **Procedure:** Start the chain with operations `value + 1` and `value * 2`, then
 *   an executor-inheriting identity node; wait for completion.
 * - **Expected:** Successful result with value `8`; first-node thread differs from
 *   the calling thread; explicit and inherited control nodes and completion use the calling thread.
 * - **Failure hints:** Check the executor captured by `Chain::link()` for node-thread mismatches and `Chain::start()` for completion-thread mismatches.
 */
void routes_work_and_completion_to_their_executors();
TEST(Chain, RoutesWorkAndCompletionToTheirExecutors)
{
    routes_work_and_completion_to_their_executors();
}

/* ## Arguments and return values */

/*
 * ### Earlier tokens remain usable and void tokens add no argument
 *
 * - **Objective:** Retention of earlier results, repeated-token support, and void-argument omission.
 * - **Input:** Seed value `7`; tokens for the seed, a void result, and string `"text"`;
 *   current value `4`.
 * - **Procedure:** Execute a successor using the seed token twice, the void token,
 *   and the string token.
 * - **Expected:** Successor arguments `(4, 7, "text", 7)`; successful result with value `22`.
 * - **Failure hints:** Check token-state retention in `Chain::link()` and `common.h` argument-tuple construction; individual argument assertions distinguish ordering from lifetime errors.
 */
void tokens_retain_earlier_values_and_omit_void_arguments();
TEST(Chain, TokensRetainEarlierValuesAndOmitVoidArguments)
{
    tokens_retain_earlier_values_and_omit_void_arguments();
}

/*
 * ### Each return form is normalized independently
 *
 * - **Objective:** Normalization of one synchronous or coroutine return form per named case.
 * - **Input:** Void, value, move-only, tuple, status, expected, and coroutine returns; nonzero and zero-code failures; error-code and nested-expected payloads.
 * - **Procedure:** Execute one node; check its normalized chain type and its exact result or payload.
 * - **Expected:** Only the outer status or expected wrapper is interpreted; successful payloads retain their value and shape; explicit failures retain their error.
 * - **Failure hints:** Use the named return form to inspect `common.h`: `normalized_value_traits` for type errors, `as_task_result()` for status or payload errors, and `invoke_task()` for coroutine-only failures.
 */
void normalizes_one_return_form(const ReturnCase & scenario);
TEST_P(ChainReturns, NormalizesOneReturnForm)
{
    normalizes_one_return_form(GetParam());
}

/*
 * ### Move-only callables preserve ordinary tuples as single arguments
 *
 * - **Objective:** Move-only callable support and preservation of ordinary tuple arguments.
 * - **Input:** Seed value `1`; callable owning a `unique_ptr` containing `4`;
 *   successor accepting a single `const std::tuple<int, int> &` argument.
 * - **Procedure:** Execute the callable to produce tuple `(1, 4)`; execute the successor
 *   to sum its elements.
 * - **Expected:** Successful result with value `5`; the tuple is accepted as one argument.
 * - **Failure hints:** Check callable ownership in `Chain::link()` and `result_to_arguments_traits` in `common.h`; ordinary tuples must remain one argument.
 */
void supports_move_only_callables_and_keeps_ordinary_tuples_intact();
TEST(Chain, SupportsMoveOnlyCallablesAndKeepsOrdinaryTuplesIntact)
{
    supports_move_only_callables_and_keeps_ordinary_tuples_intact();
}

/* ## Ownership and lifetime */

/*
 * ### A temporary chain remains alive through completion
 *
 * - **Objective:** Execution ownership after the chain expression is destroyed.
 * - **Input:** A temporary chain seeded with `23` on one context.
 * - **Procedure:** Start an increment node; destroy the temporary before running its context.
 * - **Expected:** One successful completion with value `24`.
 * - **Failure hints:** Check the state handle transferred into `Chain::start()` and retained by `execute_result()` after the caller destroys the temporary.
 */
void temporary_start_owns_its_state();
TEST(Chain, TemporaryStartOwnsItsState)
{
    temporary_start_owns_its_state();
}

/*
 * ### Completed nodes release captures while their result tokens remain alive
 *
 * - **Objective:** Task-capture release on success, returned failure, and thrown exception for synchronous and suspended tasks.
 * - **Input:** A task owning shared value `19`; a retained result token; synchronous or coroutine execution; outcome success, returned failure, or exception.
 * - **Procedure:** Release the external shared owner and execute the node while retaining its result and token.
 * - **Expected:** The selected result or error is published; the weak capture owner expires before the result and token are destroyed.
 * - **Failure hints:** Check the lifetime of the moved operation and coroutine captures in `Chain::execute()`; the named outcome identifies the release path.
 */
void completed_nodes_release_captures(const CaptureCase & scenario);
TEST_P(ChainCaptureLifetime, ReleasesCapturesWithRetainedResults)
{
    completed_nodes_release_captures(GetParam());
}

/* ## Failure propagation */

/*
 * ### Returned failures preserve their error and skip successors
 *
 * - **Objective:** Failure propagation and suppression of downstream success callbacks.
 * - **Input:** A node returning failure `invalid_argument`; successor invocation count `0`.
 * - **Procedure:** Execute the chain and wait for completion.
 * - **Expected:** Failed final result with error `invalid_argument`; successor invocation count `0`.
 * - **Failure hints:** Check the failed-predecessor branch in `Chain::link()`; error mismatch concerns propagation, while a nonzero counter means the successor was invoked.
 */
void returned_failure_skips_successors();
TEST(Chain, ReturnedFailureSkipsSuccessors)
{
    returned_failure_skips_successors();
}

/*
 * ### Task exceptions publish the same failure to direct and downstream tokens
 *
 * - **Objective:** Failure publication for synchronous and suspended tasks, independently of exception category.
 * - **Input:** A task throwing `system_error(invalid_argument)` or `runtime_error`, synchronously or after suspension; a skipped successor and retained tokens for both nodes.
 * - **Procedure:** Run the tail; observe the throwing node and skipped tail through separate junctions.
 * - **Expected:** Every result has error `invalid_argument` or `io_error`, respectively; no success callback runs.
 * - **Failure hints:** Check `Chain::execute()`, `ExecutionState::publish()`, and `details::exception_error()`; the trace distinguishes the throwing state from its skipped tail.
 */
void publishes_task_exceptions(const ExceptionCase & scenario);
TEST_P(ChainTaskExceptions, PublishesFailureToNodeAndSuccessors)
{
    publishes_task_exceptions(GetParam());
}

/*
 * ### Each observed result-move exception becomes a failure
 *
 * - **Objective:** Exception conversion at every result-move position observed on the successful path.
 * - **Input:** A value with a move counter and a configurable move position at which to throw.
 * - **Procedure:** Execute a successful baseline to obtain move count `N`; repeat execution
 *   with the counter reset and an exception injected at each position `k` in `[1, N]`;
 *   observe each failed node through a junction constructed after execution.
 * - **Expected:** Baseline succeeds with `N > 0`; each injected execution fails with `io_error`
 *   and move count `k`; each later junction observes `io_error` without running its successor.
 * - **Failure hints:** Use the traced move position to inspect `spawn_result()` and `ExecutionState::publish()`; observer failures indicate the failed state was not published.
 */
void publishes_failures_from_every_result_move();
TEST(Chain, PublishesFailuresFromEveryResultMove)
{
    publishes_failures_from_every_result_move();
}

/* ## Completion boundaries */

/*
 * ### Failures are delivered once on the selected completion executor
 *
 * - **Objective:** Executor routing and single delivery of an already published task failure.
 * - **Input:** A CPU node returning `invalid_argument`; completion on a separate context pumped by the calling thread.
 * - **Procedure:** Start the node and run the completion context until its work is exhausted or one second elapses.
 * - **Expected:** One failed completion on the calling thread; the node runs on a different thread.
 * - **Failure hints:** Check the completion executor passed to `Chain::start()` and its `co_spawn` completion adapter; the completion count separates missing or duplicate delivery.
 */
void delivers_failure_on_the_completion_executor();
TEST(Chain, DeliversFailureOnTheCompletionExecutor)
{
    delivers_failure_on_the_completion_executor();
}

/*
 * ### Completion exceptions propagate to the context caller
 *
 * - **Objective:** Propagation of completion-callback exceptions to the context caller.
 * - **Input:** A successful void chain; completion callback throwing `runtime_error`.
 * - **Procedure:** Start the chain and call `io_context::run()`.
 * - **Expected:** `io_context::run()` throws `runtime_error`.
 * - **Failure hints:** Check `Chain::start()` completion invocation; exceptions from user completion code must reach `io_context::run()`.
 */
void completion_exceptions_escape_to_the_context_caller();
TEST(Chain, CompletionExceptionsEscapeToTheContextCaller)
{
    completion_exceptions_escape_to_the_context_caller();
}

/* ## Test support */

struct MoveOnlyValue
{
    explicit MoveOnlyValue(int value) : m_value(value) {}
    MoveOnlyValue(MoveOnlyValue &&) noexcept = default;
    MoveOnlyValue(const MoveOnlyValue &) = delete;
    int m_value;
};

struct ThrowingMoveValue
{
    using Self = ThrowingMoveValue;
public:
    ThrowingMoveValue(unsigned & move_count, unsigned throw_on_move) noexcept :
        m_move_count_p(&move_count), m_throw_on_move(throw_on_move) {}
    ThrowingMoveValue(const Self &) = delete;
    ThrowingMoveValue(Self && other) :
        m_move_count_p(other.m_move_count_p), m_throw_on_move(other.m_throw_on_move)
    {
        ++*m_move_count_p;
        if (*m_move_count_p == m_throw_on_move) { throw std::runtime_error("result move"); }
    }
public:
    unsigned * m_move_count_p;
    unsigned m_throw_on_move;
};

asio::awaitable<void> wait_once()
{
    asio::steady_timer timer {co_await asio::this_coro::executor};
    timer.expires_after(1ms);
    co_await timer.async_wait(asio::use_awaitable);
}

template <typename Case>
std::string case_name(const testing::TestParamInfo<Case> & info) { return info.param.m_name_p; }

void PrintTo(const ReturnCase & scenario, std::ostream * output_p) { *output_p << scenario.m_name_p; }
void PrintTo(const ExceptionCase & scenario, std::ostream * output_p) { *output_p << scenario.m_name_p; }
void PrintTo(const CaptureCase & scenario, std::ostream * output_p) { *output_p << scenario.m_name_p; }

[[noreturn]] void raise_exception(bool system_error)
{
    if (system_error) { throw std::system_error(std::make_error_code(std::errc::invalid_argument)); }
    throw std::runtime_error("task");
}

template <typename Value, typename Function, typename Check>
void expect_return(Runtime & runtime, Function function, Check check)
{
    auto chain = task::Chain {runtime.cpu()}.link(std::move(function));
    static_assert(std::same_as<typename decltype(chain)::value_type, Value>);
    auto result_rh = runtime.run(std::move(chain));
    ASSERT_TRUE(*result_rh);
    if constexpr (std::is_void_v<Value>) { check(); }
    else { check(result_rh->value()); }
}

template <typename Function>
void expect_return_failure(Runtime & runtime, Function function, std::error_code expected_error)
{
    auto result_rh = runtime.run(task::Chain {runtime.cpu()}.link(std::move(function)));
    ASSERT_FALSE(*result_rh);
    EXPECT_EQ(result_rh->error(), expected_error);
}

INSTANTIATE_TEST_SUITE_P(ReturnForms, ChainReturns, testing::Values(
    ReturnCase {"Void", [](Runtime & runtime) { expect_return<void>(runtime, [] {}, [] {}); }},
    ReturnCase {"StatusSuccess", [](Runtime & runtime) { expect_return<void>(runtime, [] { return std::error_code {}; }, [] {}); }},
    ReturnCase {"StatusFailure", [](Runtime & runtime) {
        const auto error = std::make_error_code(std::errc::invalid_argument);
        expect_return_failure(runtime, [error] { return error; }, error);
    }},
    ReturnCase {"ExpectedVoid", [](Runtime & runtime) { expect_return<void>(runtime, [] { return task::Result<void> {}; }, [] {}); }},
    ReturnCase {"ExpectedValue", [](Runtime & runtime) {
        expect_return<int>(runtime, [] { return task::Result<int> {42}; }, [](int value) { EXPECT_EQ(value, 42); });
    }},
    ReturnCase {"ExpectedVoidFailure", [](Runtime & runtime) {
        const auto error = std::make_error_code(std::errc::invalid_argument);
        expect_return_failure(runtime, [error]() -> task::Result<void> { return std::unexpected(error); }, error);
    }},
    ReturnCase {"ExpectedZeroCodeFailure", [](Runtime & runtime) {
        expect_return_failure(runtime, []() -> task::Result<int> { return std::unexpected(std::error_code {}); }, {});
    }},
    ReturnCase {"Value", [](Runtime & runtime) { expect_return<int>(runtime, [] { return 42; }, [](int value) { EXPECT_EQ(value, 42); }); }},
    ReturnCase {"MoveOnly", [](Runtime & runtime) {
        expect_return<MoveOnlyValue>(runtime, [] { return MoveOnlyValue {42}; }, [](const MoveOnlyValue & value) { EXPECT_EQ(value.m_value, 42); });
    }},
    ReturnCase {"Tuple", [](Runtime & runtime) {
        expect_return<std::tuple<int, int>>(runtime, [] { return std::tuple {3, 7}; }, [](const auto & value) {
            EXPECT_EQ(std::get<0>(value), 3);
            EXPECT_EQ(std::get<1>(value), 7);
        });
    }},
    ReturnCase {"AwaitableVoid", [](Runtime & runtime) { expect_return<void>(runtime, wait_once, [] {}); }},
    ReturnCase {"AwaitableStatusSuccess", [](Runtime & runtime) {
        expect_return<void>(runtime, []() -> asio::awaitable<std::error_code> { co_await wait_once(); co_return std::error_code {}; }, [] {});
    }},
    ReturnCase {"AwaitableExpectedVoid", [](Runtime & runtime) {
        expect_return<void>(runtime, []() -> asio::awaitable<task::Result<void>> { co_await wait_once(); co_return task::Result<void> {}; }, [] {});
    }},
    ReturnCase {"AwaitableMoveOnly", [](Runtime & runtime) {
        expect_return<std::unique_ptr<int>>(runtime, []() -> asio::awaitable<std::unique_ptr<int>> { co_await wait_once(); co_return std::make_unique<int>(42); },
            [](const std::unique_ptr<int> & value_up) { EXPECT_EQ(*value_up, 42); });
    }},
    ReturnCase {"AwaitableValue", [](Runtime & runtime) {
        expect_return<int>(runtime, []() -> asio::awaitable<int> { co_await wait_once(); co_return 42; }, [](int value) { EXPECT_EQ(value, 42); });
    }},
    ReturnCase {"AwaitableStatusFailure", [](Runtime & runtime) {
        const auto error = std::make_error_code(std::errc::invalid_argument);
        expect_return_failure(runtime, [error]() -> asio::awaitable<std::error_code> { co_await wait_once(); co_return error; }, error);
    }},
    ReturnCase {"AwaitableExpected", [](Runtime & runtime) {
        expect_return<int>(runtime, []() -> asio::awaitable<task::Result<int>> { co_await wait_once(); co_return 42; }, [](int value) { EXPECT_EQ(value, 42); });
    }},
    ReturnCase {"AwaitableExpectedFailure", [](Runtime & runtime) {
        const auto error = std::make_error_code(std::errc::invalid_argument);
        expect_return_failure(runtime, [error]() -> asio::awaitable<task::Result<int>> { co_await wait_once(); co_return std::unexpected(error); }, error);
    }},
    ReturnCase {"ErrorCodePayload", [](Runtime & runtime) {
        const auto error = std::make_error_code(std::errc::invalid_argument);
        expect_return<std::error_code>(runtime, [error]() -> task::Result<std::error_code> { return task::Result<std::error_code> {std::in_place, error}; },
            [error](const std::error_code & value) { EXPECT_EQ(value, error); });
    }},
    ReturnCase {"NestedExpectedPayload", [](Runtime & runtime) {
        const auto error = std::make_error_code(std::errc::invalid_argument);
        expect_return<task::Result<int>>(runtime, [error]() -> task::Result<task::Result<int>> { return task::Result<task::Result<int>> {std::in_place, std::unexpected(error)}; },
            [error](const task::Result<int> & value) { ASSERT_FALSE(value); EXPECT_EQ(value.error(), error); });
    }}), case_name<ReturnCase>);

INSTANTIATE_TEST_SUITE_P(ExceptionForms, ChainTaskExceptions, testing::Values(
    ExceptionCase {"SynchronousSystemError", false, true},
    ExceptionCase {"SynchronousUnknownException", false, false},
    ExceptionCase {"CoroutineSystemError", true, true},
    ExceptionCase {"CoroutineUnknownException", true, false}), case_name<ExceptionCase>);

INSTANTIATE_TEST_SUITE_P(Outcomes, ChainCaptureLifetime, testing::Values(
    CaptureCase {"SynchronousSuccess", 0u, false},
    CaptureCase {"SynchronousReturnedFailure", 1u, false},
    CaptureCase {"SynchronousException", 2u, false},
    CaptureCase {"CoroutineSuccess", 0u, true},
    CaptureCase {"CoroutineReturnedFailure", 1u, true},
    CaptureCase {"CoroutineException", 2u, true}), case_name<CaptureCase>);

/* ## Test implementations */

/* ### Scheduling */

void routes_work_and_completion_to_their_executors()
{
    Runtime runtime;
    const auto render_thread = std::this_thread::get_id();
    std::thread::id worker_thread;
    std::thread::id successor_thread;
    std::thread::id inherited_thread;
    bool completed = false;
    auto chain = task::Chain {3, runtime.cpu()}
        .link([&](const int & value) {
            worker_thread = std::this_thread::get_id();
            return value + 1;
        })
        .link(runtime.control(), [&](const int & value) {
            successor_thread = std::this_thread::get_id();
            return value * 2;
        })
        .link([&](const int & value) {
            inherited_thread = std::this_thread::get_id();
            return value;
        });
    int output = 0;
    std::thread::id completion_thread;
    std::move(chain).start(runtime.control(), [&](const task::Result<int> & result) {
        completed = true;
        ASSERT_TRUE(result);
        output = result.value();
        completion_thread = std::this_thread::get_id();
    });
    runtime.pumpUntil([&] { return completed; });
    EXPECT_EQ(output, 8);
    EXPECT_NE(worker_thread, render_thread);
    EXPECT_EQ(successor_thread, render_thread);
    EXPECT_EQ(inherited_thread, render_thread);
    EXPECT_EQ(completion_thread, render_thread);
}

/* ### Arguments and return values */

void tokens_retain_earlier_values_and_omit_void_arguments()
{
    Runtime runtime;
    auto seed = task::Chain {7, runtime.cpu()};
    const task::Chain<int>::ResultToken input_token = seed.getResultToken();
    auto empty = std::move(seed).link([](const int &) {});
    auto empty_token = empty.getResultToken();
    auto text = std::move(empty).link([] { return std::string {"text"}; });
    auto text_token = text.getResultToken();
    auto value = std::move(text).link([](const std::string & value) { return static_cast<int>(value.size()); });
    auto tail = std::move(value).link(
        [](const int & current, const int & input, const std::string & text, const int & repeated) {
            EXPECT_EQ(current, 4);
            EXPECT_EQ(input, 7);
            EXPECT_EQ(text, "text");
            EXPECT_EQ(&input, &repeated);
            return current + input + static_cast<int>(text.size()) + repeated;
        }, input_token, std::move(empty_token), text_token, input_token);
    auto result_rh = runtime.run(std::move(tail));
    ASSERT_TRUE(*result_rh);
    EXPECT_EQ(result_rh->value(), 22);
}

void normalizes_one_return_form(const ReturnCase & scenario)
{
    Runtime runtime;
    scenario.m_verify_p(runtime);
}

void supports_move_only_callables_and_keeps_ordinary_tuples_intact()
{
    Runtime runtime;
    auto chain = task::Chain {1, runtime.cpu()}
        .link([value_up = std::make_unique<int>(4)](const int & input) { return std::tuple {input, *value_up}; })
        .link([](const std::tuple<int, int> & input) { return std::get<0>(input) + std::get<1>(input); });
    auto result_rh = runtime.run(std::move(chain));
    ASSERT_TRUE(*result_rh);
    EXPECT_EQ(result_rh->value(), 5);
}

/* ### Ownership and lifetime */

void temporary_start_owns_its_state()
{
    asio::io_context context;
    std::optional<int> output_opt;
    task::Chain {23, context.get_executor()}.link([](const int & value) { return value + 1; })
        .start(context.get_executor(), [&](const task::Result<int> & result) {
            ASSERT_TRUE(result);
            output_opt = result.value();
        });
    context.run();
    ASSERT_TRUE(output_opt);
    EXPECT_EQ(*output_opt, 24);
}

void completed_nodes_release_captures(const CaptureCase & scenario)
{
    Runtime runtime;
    auto owner_sp = std::make_shared<int>(19);
    std::weak_ptr<int> owner_wp = owner_sp;
    auto function = [owner_sp, outcome = scenario.m_outcome]() -> task::Result<int> {
        if (outcome == 2u) { throw std::runtime_error("task"); }
        if (outcome == 1u) { return std::unexpected(std::make_error_code(std::errc::invalid_argument)); }
        return *owner_sp;
    };
    auto chain = [&] {
        if (scenario.m_coroutine) {
            return task::Chain {runtime.cpu()}.link([function = std::move(function)]() noexcept -> asio::awaitable<task::Result<int>> {
                co_await wait_once();
                co_return function();
            });
        }
        return task::Chain {runtime.cpu()}.link(std::move(function));
    }();
    [[maybe_unused]] auto token = chain.getResultToken();
    owner_sp.reset();
    auto result_rh = runtime.run(std::move(chain));
    EXPECT_TRUE(owner_wp.expired());
    if (scenario.m_outcome == 0u) {
        ASSERT_TRUE(*result_rh);
        EXPECT_EQ(result_rh->value(), 19);
        return;
    }
    ASSERT_FALSE(*result_rh);
    EXPECT_EQ(result_rh->error(), std::make_error_code(scenario.m_outcome == 1u ? std::errc::invalid_argument : std::errc::io_error));
}

/* ### Failure propagation */

void returned_failure_skips_successors()
{
    Runtime runtime;
    const auto failure = std::make_error_code(std::errc::invalid_argument);
    std::atomic_uint forbidden_runs = 0;
    auto failed = task::Chain {runtime.cpu()}.link([&]() -> task::Result<int> { return std::unexpected(failure); });
    auto chain = std::move(failed).link([&](const int &) { ++forbidden_runs; return 0; });
    auto failed_rh = runtime.run(std::move(chain));
    ASSERT_FALSE(*failed_rh);
    EXPECT_EQ(failed_rh->error(), failure);
    EXPECT_EQ(forbidden_runs, 0u);
}

void publishes_task_exceptions(const ExceptionCase & scenario)
{
    Runtime runtime;
    const auto expected_error = std::make_error_code(scenario.m_system_error ? std::errc::invalid_argument : std::errc::io_error);
    auto make_chain = [&] {
        if (scenario.m_coroutine) {
            return task::Chain {runtime.cpu()}.link([&]() noexcept -> asio::awaitable<int> {
                co_await wait_once();
                raise_exception(scenario.m_system_error);
            });
        }
        return task::Chain {runtime.cpu()}.link([&]() -> int { raise_exception(scenario.m_system_error); });
    };
    unsigned successor_runs = 0;
    auto throwing = make_chain();
    auto throwing_token = throwing.getResultToken();
    auto tail = std::move(throwing).link([&](const int &) { ++successor_runs; return 0; });
    auto tail_token = tail.getResultToken();
    auto result_rh = runtime.run(std::move(tail));
    ASSERT_FALSE(*result_rh);
    EXPECT_EQ(result_rh->error(), expected_error);
    for (const auto & [stage, token] : {std::pair {"throwing node", throwing_token}, std::pair {"skipped tail", tail_token}}) {
        SCOPED_TRACE(stage);
        auto observed = task::merge(token).link(runtime.cpu(), [&](const int &) { ++successor_runs; });
        auto observed_rh = runtime.run(std::move(observed));
        ASSERT_FALSE(*observed_rh);
        EXPECT_EQ(observed_rh->error(), expected_error);
    }
    EXPECT_EQ(successor_runs, 0u);
}

void publishes_failures_from_every_result_move()
{
    Runtime runtime;
    unsigned moves = 0;
    auto make_chain = [&](unsigned throw_on_move) {
        return task::Chain {runtime.cpu()}.link([&moves, throw_on_move] {
            return ThrowingMoveValue {moves, throw_on_move};
        });
    };
    auto result_rh = runtime.run(make_chain(0));
    ASSERT_TRUE(*result_rh);
    const auto move_count = moves;
    ASSERT_GT(move_count, 0u);
    for (unsigned throw_on_move = 1; throw_on_move <= move_count; ++throw_on_move) {
        SCOPED_TRACE(throw_on_move);
        moves = 0;
        auto chain = make_chain(throw_on_move);
        auto token = chain.getResultToken();
        auto failed_rh = runtime.run(std::move(chain));
        ASSERT_FALSE(*failed_rh);
        EXPECT_EQ(failed_rh->error(), std::make_error_code(std::errc::io_error));
        EXPECT_EQ(moves, throw_on_move);
        bool successor_ran = false;
        auto observed = task::merge(token).link(runtime.cpu(), [&](const ThrowingMoveValue &) { successor_ran = true; });
        auto observed_rh = runtime.run(std::move(observed));
        ASSERT_FALSE(*observed_rh);
        EXPECT_EQ(observed_rh->error(), std::make_error_code(std::errc::io_error));
        EXPECT_FALSE(successor_ran);
        EXPECT_EQ(moves, throw_on_move);
    }
}

/* ### Completion boundaries */

void delivers_failure_on_the_completion_executor()
{
    Runtime runtime;
    asio::io_context completion_context;
    const auto caller = std::this_thread::get_id();
    const auto failure = std::make_error_code(std::errc::invalid_argument);
    unsigned completions = 0;
    auto chain = task::Chain {runtime.cpu()}.link([&]() -> task::Result<void> {
        EXPECT_NE(std::this_thread::get_id(), caller);
        return std::unexpected(failure);
    });
    std::move(chain).start(completion_context.get_executor(), [&](const task::Result<void> & result) {
        ++completions;
        EXPECT_EQ(std::this_thread::get_id(), caller);
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error(), failure);
    });
    completion_context.run_for(1s);
    EXPECT_EQ(completions, 1u);
}

void completion_exceptions_escape_to_the_context_caller()
{
    asio::io_context context;
    auto chain = task::Chain {context.get_executor()}.link([] {});
    std::move(chain).start(context.get_executor(), [](const task::Result<void> &) { throw std::runtime_error("completion"); });
    EXPECT_THROW(context.run(), std::runtime_error);
}

}
