#include <async/execution.h>
#include <async/task.h>
#include <gtest/gtest.h>
#include <vector>

namespace {

namespace runtime = lcf::async::execution;
namespace stde = stdexec;

/*
 * Objective: a count budget leaves excess ready callbacks queued.
 * Input: ten callbacks and a budget of three.
 * Procedure: poll twice, inspect ready counts, then drain the remaining callbacks.
 * Expected: 3, 3, and 4 callbacks; FIFO order and a final empty queue.
 * Failure hints: inspect Context::poll and Queue's ready count under its mutex.
 */
void count_budget_limits_admission();
TEST(StdexecPoll, CountBudgetLimitsAdmission) { count_budget_limits_admission(); }

/*
 * Objective: zero count or time budgets execute no callbacks.
 * Input: one queued callback and each zero budget.
 * Procedure: poll under both limits, then call pollOne.
 * Expected: both reports contain zero callbacks; pollOne finally executes the callback.
 * Failure hints: inspect budget checks before dequeue rather than after invocation.
 */
void empty_budgets_keep_callbacks_ready();
TEST(StdexecPoll, EmptyBudgetsKeepCallbacksReady) { empty_budgets_keep_callbacks_ready(); }

/*
 * Objective: time budgets stop admission after an overrun without preempting user code.
 * Input: a 20ms callback, another ready callback, and a 1ms budget.
 * Procedure: poll until the first callback executes; bound the overall wait at five seconds.
 * Expected: the callback finishes; the report exceeds its time budget; the second stays queued.
 * Failure hints: inspect the clock check between callbacks; this is a cooperative budget.
 */
void slow_callback_reports_cooperative_overrun();
TEST(StdexecPoll, SlowCallbackReportsCooperativeOverrun) { slow_callback_reports_cooperative_overrun(); }

/*
 * Objective: fanout notification itself respects the queue's cooperative boundaries.
 * Input: 64 receivers waiting on one signal bound to the control executor.
 * Procedure: signal once, then poll with a one-callback budget 64 times.
 * Expected: signalling queues one step; each poll notifies exactly one receiver.
 * Failure hints: inspect Notification::dispatchNext and the registration's queued ownership.
 */
void fanout_notifications_are_incremental();
TEST(StdexecPoll, FanoutNotificationsAreIncremental) { fanout_notifications_are_incremental(); }

/*
 * Objective: separate merge descriptors do not enqueue their entire fanout at once.
 * Input: 64 suspended branches, each with its own descriptor for the same source.
 * Procedure: start the source and admit one callback at a time.
 * Expected: every branch completes; notification publication never creates a 64-item burst.
 * Failure hints: inspect the source publication queue and SourceObserver::invoke.
 */
void source_observers_are_incremental();
TEST(StdexecPoll, SourceObserversAreIncremental) { source_observers_are_incremental(); }

/*
 * Objective: a failure propagates through a long chain in cooperative steps.
 * Input: 128 successors after a failing first task.
 * Procedure: poll with a one-callback budget until the final failure arrives.
 * Expected: successors never execute; propagation takes at least one admission per successor.
 * Failure hints: inspect Chain::link's resume boundary after awaiting its predecessor.
 */
void failure_propagation_keeps_queue_boundaries();
TEST(StdexecPoll, FailurePropagationKeepsQueueBoundaries) { failure_propagation_keeps_queue_boundaries(); }

/*
 * Objective: stop halts admission even when callbacks remain queued.
 * Input: a callback that stops its context followed by a second callback.
 * Procedure: run, inspect the pending callback, restart and poll again.
 * Expected: stop returns after the first; restart admits the remaining callback.
 * Failure hints: inspect Queue::pop's stopped check before dequeue.
 */
void stop_preserves_queued_callbacks_until_restart();
TEST(StdexecPoll, StopPreservesQueuedCallbacksUntilRestart) { stop_preserves_queued_callbacks_until_restart(); }

/*
 * Objective: explicit yield keeps a queue boundary after synchronous child returns are optimized.
 * Input: one task executing three stages with a yield between adjacent stages.
 * Procedure: poll one callback at a time and record the number of stage advances.
 * Expected: no stages during start, at most one advance per poll, and all stages finish.
 * Failure hints: inspect YieldAwaitable's direct schedule and task completion affinity.
 */
void explicit_yield_keeps_queue_boundaries();
TEST(StdexecPoll, ExplicitYieldKeepsQueueBoundaries) { explicit_yield_keeps_queue_boundaries(); }

struct CountReceiver
{
    using receiver_concept = stde::receiver_t;
    void set_value() noexcept { ++*m_calls_p; }
    void set_error(std::exception_ptr) noexcept { ++*m_failures_p; }
    unsigned * m_calls_p;
    unsigned * m_failures_p;
};

void count_budget_limits_admission()
{
    runtime::Context context;
    unsigned next = 0;
    for (unsigned index = 0; index < 10; ++index) {
        context.getExecutor().post([&, index] { EXPECT_EQ(next++, index); });
    }
    EXPECT_EQ(context.getReadyCount(), 10u);
    const auto first = context.poll({.m_max_callbacks = 3});
    EXPECT_EQ(first.m_callbacks, 3u);
    EXPECT_TRUE(first.m_count_limit_reached);
    EXPECT_EQ(context.getReadyCount(), 7u);
    EXPECT_EQ(context.poll({.m_max_callbacks = 3}).m_callbacks, 3u);
    EXPECT_EQ(context.poll().m_callbacks, 4u);
    EXPECT_EQ(context.getReadyCount(), 0u);
    EXPECT_EQ(next, 10u);
}

void empty_budgets_keep_callbacks_ready()
{
    runtime::Context context;
    unsigned calls = 0;
    context.getExecutor().post([&] { ++calls; });
    EXPECT_EQ(context.poll({.m_max_callbacks = 0}).m_callbacks, 0u);
    EXPECT_EQ(context.poll({.m_max_duration = std::chrono::milliseconds {0}}).m_callbacks, 0u);
    EXPECT_EQ(context.getReadyCount(), 1u);
    EXPECT_EQ(calls, 0u);
    EXPECT_EQ(context.pollOne(), 1u);
    EXPECT_EQ(calls, 1u);
}

void slow_callback_reports_cooperative_overrun()
{
    runtime::Context context;
    unsigned calls = 0;
    context.getExecutor().post([&] {
        const auto finish = std::chrono::steady_clock::now() + std::chrono::milliseconds {20};
        while (std::chrono::steady_clock::now() < finish) { std::this_thread::yield(); }
        ++calls;
    });
    context.getExecutor().post([&] { ++calls; });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds {5};
    runtime::PollReport report;
    do {
        report = context.poll({.m_max_duration = std::chrono::milliseconds {1}});
        ASSERT_LT(std::chrono::steady_clock::now(), deadline);
    } while (report.m_callbacks == 0u);
    EXPECT_EQ(report.m_callbacks, 1u);
    EXPECT_EQ(calls, 1u);
    EXPECT_TRUE(report.m_time_limit_reached);
    EXPECT_GE(report.m_elapsed, std::chrono::milliseconds {20});
    EXPECT_EQ(context.getReadyCount(), 1u);
    EXPECT_EQ(context.pollOne(), 1u);
}

void fanout_notifications_are_incremental()
{
    runtime::Context context;
    runtime::Signal signal {context.getExecutor()};
    unsigned calls = 0;
    unsigned failures = 0;
    using Operation = stde::connect_result_t<decltype(signal.wait()), CountReceiver>;
    std::vector<std::unique_ptr<Operation>> operations;
    for (unsigned index = 0; index < 64; ++index) {
        operations.emplace_back(new Operation(stde::connect(signal.wait(), CountReceiver {&calls, &failures})));
        stde::start(*operations.back());
    }
    EXPECT_EQ(context.getReadyCount(), 0u);
    ASSERT_TRUE(signal.set());
    EXPECT_EQ(calls, 0u);
    EXPECT_EQ(context.getReadyCount(), 1u);
    for (unsigned index = 0; index < 64; ++index) {
        SCOPED_TRACE(index);
        EXPECT_EQ(context.poll({.m_max_callbacks = 1}).m_callbacks, 1u);
        EXPECT_EQ(calls, index + 1);
        EXPECT_EQ(context.getReadyCount(), index == 63 ? 0u : 1u);
    }
    EXPECT_EQ(failures, 0u);
}

void source_observers_are_incremental()
{
    namespace task = lcf::async::task;
    runtime::Context context;
    const auto executor = context.getExecutor();
    auto source = task::Chain {executor}.link([] { return 42; });
    const auto token = source.getResultToken();
    unsigned completions = 0;
    for (unsigned index = 0; index < 64; ++index) {
        auto branch = task::merge(token).link(executor, [](const int & value) { return value; });
        std::move(branch).start(executor, [&](const task::Result<int> & result) {
            ++completions;
            ASSERT_TRUE(result);
            EXPECT_EQ(result.value(), 42);
        });
    }
    context.poll();
    EXPECT_EQ(completions, 0u);
    std::move(source).start(executor, [](const auto &) {});
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds {10};
    while (completions != 64u) {
        EXPECT_LE(context.getReadyCount(), 16u);
        context.poll({.m_max_callbacks = 1});
        ASSERT_LT(std::chrono::steady_clock::now(), deadline);
    }
    context.poll();
}

void failure_propagation_keeps_queue_boundaries()
{
    namespace task = lcf::async::task;
    runtime::Context context;
    const auto executor = context.getExecutor();
    unsigned successor_runs = 0;
    auto chain = task::Chain {executor}.link([]() -> task::Result<int> {
        return std::unexpected(std::make_error_code(std::errc::io_error));
    });
    for (unsigned index = 0; index < 128; ++index) {
        chain = std::move(chain).link([&](const int & value) { ++successor_runs; return value; });
    }
    bool completed = false;
    std::move(chain).start(executor, [&](const task::Result<int> & result) {
        completed = true;
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error(), std::errc::io_error);
    });
    std::size_t callbacks = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds {10};
    while (not completed) {
        callbacks += context.poll({.m_max_callbacks = 1}).m_callbacks;
        ASSERT_LT(std::chrono::steady_clock::now(), deadline);
    }
    EXPECT_EQ(successor_runs, 0u);
    EXPECT_GE(callbacks, 128u);
}

void stop_preserves_queued_callbacks_until_restart()
{
    runtime::Context context;
    unsigned calls = 0;
    context.getExecutor().post([&] { ++calls; context.stop(); });
    context.getExecutor().post([&] { ++calls; });
    context.run();
    EXPECT_EQ(calls, 1u);
    EXPECT_EQ(context.getReadyCount(), 1u);
    EXPECT_EQ(context.pollOne(), 0u);
    context.restart();
    EXPECT_EQ(context.pollOne(), 1u);
    EXPECT_EQ(calls, 2u);
}

void explicit_yield_keeps_queue_boundaries()
{
    namespace task = lcf::async::task;
    runtime::Context context;
    unsigned stages = 0;
    bool done = false;
    auto chain = task::Chain {context.getExecutor()}.link([&]() -> runtime::Awaitable<void> {
        ++stages;
        co_await runtime::yield();
        ++stages;
        co_await runtime::yield();
        ++stages;
    });
    std::move(chain).start(context.getExecutor(), [&](const task::Result<void> & result) {
        done = true;
        EXPECT_TRUE(result);
    });
    EXPECT_EQ(stages, 0u);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds {10};
    while (not done) {
        const auto before = stages;
        context.pollOne();
        ASSERT_LE(stages - before, 1u);
        ASSERT_LT(std::chrono::steady_clock::now(), deadline);
    }
    EXPECT_EQ(stages, 3u);
}

}
