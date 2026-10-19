#include <Runtime.h>
#include <async/core.h>
#include <gtest/gtest.h>
#include <atomic>

namespace {

namespace core = lcf::async;
namespace task = core::task;
using core::testing::Runtime;
using namespace std::chrono_literals;

class SignalBinding : public ::testing::TestWithParam<bool> {};

/*
 * Objective: post queues work instead of invoking it during submission.
 * Input: one callback and an idle context.
 * Procedure: submit through core::post, inspect state, then poll the context.
 * Expected: zero inline calls and one eventual call on the polling thread.
 * Failure hints: inspect the core post adapter and the selected executor.
 */
void post_defers_callback();
TEST(Core, PostDefersCallback) { post_defers_callback(); }

/*
 * Objective: yield separates two coroutine stages by an executor admission.
 * Input: a control-thread task with two stages around core::yield.
 * Procedure: start it and poll one handler at a time.
 * Expected: no inline work and no poll advances both stages.
 * Failure hints: inspect the yield operation rather than assuming co_await queues.
 */
void yield_defers_continuation();
TEST(Core, YieldDefersContinuation) { yield_defers_continuation(); }

/*
 * Objective: a signal broadcasts once and retains its completed state.
 * Input: four pending waiters and one waiter added after set.
 * Procedure: register the first group, set twice, then add the late waiter.
 * Expected: only the first set succeeds; all five waiters complete once.
 * Failure hints: inspect close-as-notification and the persistent ready state.
 */
void signal_broadcasts_to_current_and_future_waiters(bool bound);
TEST_P(SignalBinding, BroadcastsToCurrentAndFutureWaiters) { signal_broadcasts_to_current_and_future_waiters(GetParam()); }

/*
 * Objective: cancelling one wait leaves other waiters and the signal usable.
 * Input: two pending waits, one with its own cancellation source.
 * Procedure: request cancellation, observe its failure, then set the signal.
 * Expected: the other waiter stays pending until set and then succeeds.
 * Failure hints: inspect per-operation cancellation instead of cancelling the channel.
 */
void cancelling_one_waiter_preserves_signal();
TEST(Core, CancellingOneWaiterPreservesSignal) { cancelling_one_waiter_preserves_signal(); }

/*
 * Objective: setting concurrently with wait registration cannot lose a wakeup.
 * Input: 64 fresh signals, a waiter, and a producer thread for each.
 * Procedure: race the producer's set with control-thread registration and join it.
 * Expected: exactly one successful task completion in every iteration.
 * Failure hints: inspect the locked decision between subscribing and becoming ready.
 */
void set_races_wait_registration(bool bound);
TEST_P(SignalBinding, SetRacesWaitRegistration) { set_races_wait_registration(GetParam()); }

INSTANTIATE_TEST_SUITE_P(ExecutorBinding, SignalBinding, ::testing::Bool(),
    [](const ::testing::TestParamInfo<bool> & info) { return info.param ? "Bound" : "Unbound"; });

/*
 * Objective: public cancellation interrupts a long core delay.
 * Input: a one-hour delay followed by another stage.
 * Procedure: wait until the task enters, request cancellation, and pump completion.
 * Expected: the backend's timer cancellation error; neither later stage runs.
 * Failure hints: inspect cancellation association at start and the delay adapter.
 */
void delay_cancellation_skips_successor();
TEST(Core, DelayCancellationSkipsSuccessor) { delay_cancellation_skips_successor(); }

void post_defers_callback()
{
    core::testing::Context context;
    const auto thread_id = std::this_thread::get_id();
    unsigned calls = 0;
    core::post(context.get_executor(), [&] { ++calls; EXPECT_EQ(std::this_thread::get_id(), thread_id); });
    EXPECT_EQ(calls, 0u);
    context.poll();
    EXPECT_EQ(calls, 1u);
}

void yield_defers_continuation()
{
    core::testing::Context context;
    unsigned stages = 0;
    unsigned completions = 0;
    auto chain = task::Chain {context.get_executor()}.link([&]() -> core::Awaitable<void> {
        ++stages;
        co_await core::yield();
        ++stages;
    });
    std::move(chain).start(context.get_executor(), [&](const task::Result<void> & result) {
        ++completions;
        EXPECT_TRUE(result);
    });
    EXPECT_EQ(stages, 0u);
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (completions == 0u) {
        const auto previous = stages;
        context.poll_one();
        EXPECT_LE(stages - previous, 1u);
        ASSERT_LT(std::chrono::steady_clock::now(), deadline);
    }
    EXPECT_EQ(stages, 2u);
    EXPECT_EQ(completions, 1u);
}

void signal_broadcasts_to_current_and_future_waiters(bool bound)
{
    Runtime runtime;
    auto signal_up = bound ? std::make_unique<core::Signal>(runtime.control()) : std::make_unique<core::Signal>();
    auto & signal = *signal_up;
    const auto thread_id = std::this_thread::get_id();
    unsigned entered = 0;
    unsigned released = 0;
    unsigned completions = 0;
    const auto start_waiter = [&] {
        auto chain = task::Chain {runtime.control()}.link([&]() -> core::Awaitable<void> {
            ++entered;
            co_await signal.wait();
            EXPECT_EQ(std::this_thread::get_id(), thread_id);
            ++released;
        });
        std::move(chain).start(runtime.control(), [&](const task::Result<void> & result) {
            ++completions;
            EXPECT_TRUE(result);
        });
    };
    for (unsigned index = 0; index < 4; ++index) { start_waiter(); }
    runtime.pumpUntil([&] { return entered == 4u; });
    EXPECT_EQ(released, 0u);
    EXPECT_TRUE(signal.set());
    EXPECT_FALSE(signal.set());
    start_waiter();
    runtime.pumpUntil([&] { return completions == 5u; });
    EXPECT_EQ(entered, 5u);
    EXPECT_EQ(released, 5u);
}

void cancelling_one_waiter_preserves_signal()
{
    Runtime runtime;
    core::Signal signal {runtime.control()};
    core::CancellationSource source;
    unsigned entered = 0;
    unsigned cancelled = 0;
    unsigned completed = 0;
    const auto wait = [&]() -> core::Awaitable<void> { ++entered; co_await signal.wait(); };
    auto first = task::Chain {runtime.control()}.link(wait);
    auto second = task::Chain {runtime.control()}.link(wait);
    std::move(first).start(runtime.control(), core::bind_cancellation(source, [&](const task::Result<void> & result) {
        ++cancelled;
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error(), core::testing::wait_cancelled_error());
    }));
    std::move(second).start(runtime.control(), [&](const task::Result<void> & result) {
        ++completed;
        EXPECT_TRUE(result);
    });
    runtime.pumpUntil([&] { return entered == 2u; });
    core::request_cancellation(source);
    runtime.pumpUntil([&] { return cancelled == 1u; });
    EXPECT_EQ(completed, 0u);
    EXPECT_TRUE(signal.set());
    runtime.pumpUntil([&] { return completed == 1u; });
    EXPECT_EQ(cancelled, 1u);
    EXPECT_FALSE(signal.set());
}

void set_races_wait_registration(bool bound)
{
    Runtime runtime;
    const auto thread_id = std::this_thread::get_id();
    for (unsigned iteration = 0; iteration < 64; ++iteration) {
        SCOPED_TRACE(iteration);
        auto signal_up = bound ? std::make_unique<core::Signal>(runtime.control()) : std::make_unique<core::Signal>();
        auto & signal = *signal_up;
        unsigned completions = 0;
        std::atomic_bool set = false;
        auto chain = task::Chain {runtime.control()}.link([&]() -> core::Awaitable<void> {
            co_await signal.wait();
            EXPECT_EQ(std::this_thread::get_id(), thread_id);
        });
        std::move(chain).start(runtime.control(), [&](const task::Result<void> & result) {
            ++completions;
            EXPECT_TRUE(result);
        });
        std::jthread producer {[&] { set.store(signal.set(), std::memory_order_release); }};
        runtime.pumpUntil([&] { return completions != 0u; });
        producer.join();
        EXPECT_TRUE(set.load(std::memory_order_acquire));
        EXPECT_EQ(completions, 1u);
        EXPECT_FALSE(signal.set());
    }
}

void delay_cancellation_skips_successor()
{
    Runtime runtime;
    core::CancellationSource source;
    bool entered = false;
    bool resumed = false;
    bool successor = false;
    unsigned completions = 0;
    auto chain = task::Chain {runtime.control()}
        .link([&]() -> core::Awaitable<void> {
            entered = true;
            co_await core::delay(1h);
            resumed = true;
        })
        .link([&] { successor = true; });
    std::move(chain).start(runtime.control(), core::bind_cancellation(source, [&](const task::Result<void> & result) {
        ++completions;
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error(), core::testing::timer_cancelled_error());
    }));
    runtime.pumpUntil([&] { return entered; });
    core::request_cancellation(source);
    runtime.pumpUntil([&] { return completions != 0u; });
    EXPECT_EQ(completions, 1u);
    EXPECT_FALSE(resumed);
    EXPECT_FALSE(successor);
}

}
