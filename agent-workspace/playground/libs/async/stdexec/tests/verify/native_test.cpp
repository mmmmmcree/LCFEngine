#include <async/execution.h>
#include <Runtime.h>
#include <gtest/gtest.h>
#include <atomic>
#include <stop_token>

namespace stde = stdexec;

namespace {

namespace task = lcf::async::task;
namespace runtime = lcf::async::execution;

/*
 * Objective: native stopped completion publishes failure without calling on_error.
 * Input: a task awaiting just_stopped(), a recovery handler, and a retained token.
 * Procedure: run the task, then observe its token through merge.
 * Expected: operation_canceled at both observations; no recovery or successor call.
 * Failure hints: inspect stopped_as_error in invoke_task and final publication.
 */
void stopped_task_publishes_failure();
TEST(StdexecNative, StoppedTaskPublishesFailure) { stopped_task_publishes_failure(); }

/*
 * Objective: a pre-stopped source notifies dependants and releases unused captures.
 * Input: a requested stop source, a retained producer token, and a registered join.
 * Procedure: start the waiting join, then start the cancelled producer.
 * Expected: exactly one failed completion per branch and expired execution captures.
 * Failure hints: inspect Delivery::Receiver::set_stopped and ExecutionState::notify.
 */
void pre_stopped_source_notifies_dependants();
TEST(StdexecNative, PreStoppedSourceNotifiesDependants) { pre_stopped_source_notifies_dependants(); }

/*
 * Objective: cancellation before registration cannot leave a timer pending.
 * Input: an already stopped token and a one-hour timer.
 * Procedure: connect and start without running a worker context.
 * Expected: immediate single error completion with operation_canceled.
 * Failure hints: inspect TimerService::cancel/submit and TimerRegistration::start.
 */
void pre_stopped_timer_completes();
TEST(StdexecNative, PreStoppedTimerCompletes) { pre_stopped_timer_completes(); }

/*
 * Objective: timer start and stop races have exactly one terminal event.
 * Input: one-hour timers and a stop request on another thread, repeated 64 times.
 * Procedure: race start against request_stop and join the requesting thread.
 * Expected: one cancelled completion for every iteration, without waiting an hour.
 * Failure hints: inspect the timer service's locked terminal decision and unlinking.
 */
void timer_start_races_stop();
TEST(StdexecNative, TimerStartRacesStop) { timer_start_races_stop(); }

/*
 * Objective: signal completion and cancellation safely choose one outcome.
 * Input: bound and unbound signals carrying an inplace stop token, repeated 64 times.
 * Procedure: race Signal::set against request_stop and join both threads.
 * Expected: exactly one value or cancellation, including cancellation after enqueue.
 * Failure hints: inspect NotificationRegistration::complete and subscription cleanup.
 */
void notification_races_stop();
TEST(StdexecNative, NotificationRacesStop) { notification_races_stop(); }

/*
 * Objective: a long ready chain retains an individual queue boundary per user task.
 * Input: 128 synchronous links on the control context.
 * Procedure: start, then record the number of user invocations after each pollOne.
 * Expected: no invocation during start; each poll advances at most one user task.
 * Failure hints: inspect invoke_task's yield and the queue scheduler's schedule sender.
 */
void ready_nodes_keep_queue_boundaries();
TEST(StdexecNative, ReadyNodesKeepQueueBoundaries) { ready_nodes_keep_queue_boundaries(); }

/*
 * Objective: Executor erases distinct scheduler types without losing callback semantics.
 * Input: the queue scheduler and a distinct forwarding scheduler on one context.
 * Procedure: route nodes through both, then throw from the ordinary completion.
 * Expected: final value 42; the original exception reaches the context caller.
 * Failure hints: inspect ExecutorState's scheduler and ordinary callback dispatch.
 */
void executor_accepts_distinct_callback_schedulers();
TEST(StdexecNative, ExecutorAcceptsDistinctCallbackSchedulers) { executor_accepts_distinct_callback_schedulers(); }

/*
 * Objective: Executor preserves a custom scheduler's stopped completion.
 * Input: a callback-capable scheduler whose schedule sender stops without running work.
 * Procedure: use it as the executor of a chain node and drain the control context.
 * Expected: one operation_canceled result, with no task or on_error invocation.
 * Failure hints: inspect the custom-scheduler branch of Executor::Operation.
 */
void executor_preserves_scheduler_stopped();
TEST(StdexecNative, ExecutorPreservesSchedulerStopped) { executor_preserves_scheduler_stopped(); }

/*
 * Objective: Executor preserves a custom scheduler's error completion.
 * Input: a callback-capable scheduler reporting permission_denied from schedule().
 * Procedure: use it as the executor of a chain node and drain the control context.
 * Expected: one permission_denied result, with no task or on_error invocation.
 * Failure hints: inspect the custom-scheduler branch and the chain exception boundary.
 */
void executor_preserves_scheduler_error();
TEST(StdexecNative, ExecutorPreservesSchedulerError) { executor_preserves_scheduler_error(); }

/*
 * Objective: a child task that explicitly changes schedulers restores its parent's context.
 * Input: a control-thread parent and a child rescheduled onto the worker context.
 * Procedure: await the child, record its threads, then yield again in the parent.
 * Expected: value 42, child work off control, parent continuation and next yield on control.
 * Failure hints: inspect Awaitable's borrowed context detachment and exit cleanup.
 */
void child_task_restores_parent_scheduler();
TEST(StdexecNative, ChildTaskRestoresParentScheduler) { child_task_restores_parent_scheduler(); }

/*
 * Objective: exception propagation from a rescheduled child preserves the parent context.
 * Input: a child throwing permission_denied after switching to a worker scheduler.
 * Procedure: catch in the parent, record the thread, yield once, then rethrow.
 * Expected: permission_denied with both the parent continuation and next yield on control.
 * Failure hints: inspect the child cleanup continuation and the direct task awaiter.
 */
void failed_child_restores_parent_scheduler();
TEST(StdexecNative, FailedChildRestoresParentScheduler) { failed_child_restores_parent_scheduler(); }

/*
 * Objective: an Awaitable coroutine can be consumed directly as a stdexec sender.
 * Input: a coroutine returning a unique_ptr containing 42, without a task chain.
 * Procedure: pass the returned object to sync_wait, inspect the value, then destroy it.
 * Expected: one value 42 and normal return without recursive coroutine destruction.
 * Failure hints: inspect the coroutine return object, promise, and direct sender connection.
 */
void awaitable_works_with_sync_wait();
TEST(StdexecNative, AwaitableWorksWithSyncWait) { awaitable_works_with_sync_wait(); }

/*
 * Objective: a control scheduler error publishes failure to source observers.
 * Input: an unstarted source with retained captures, a merge observer, and an erroring controller.
 * Procedure: start the observer, start the source, and drain ordinary callbacks.
 * Expected: one permission_denied completion per root, no user work, and released captures.
 * Failure hints: inspect ExecutionSender's entry error, publication, and function ownership.
 */
void control_scheduler_error_notifies_dependants();
TEST(StdexecNative, ControlSchedulerErrorNotifiesDependants) { control_scheduler_error_notifies_dependants(); }

/*
 * Objective: stopped control scheduling cannot leave a merge observer waiting.
 * Input: the same source/observer graph with a controller returning stopped.
 * Procedure: start both roots and drain the callback queue.
 * Expected: operation_canceled at both completions, no user work, and released captures.
 * Failure hints: inspect ExecutionSender's queued failure and notification publication.
 */
void stopped_control_scheduler_notifies_dependants();
TEST(StdexecNative, StoppedControlSchedulerNotifiesDependants) { stopped_control_scheduler_notifies_dependants(); }

/*
 * Objective: an exception while connecting entry scheduling still completes source observers.
 * Input: a controller throwing permission_denied from schedule-sender connect().
 * Procedure: start the source and its pre-registered observer, then drain callbacks.
 * Expected: both roots fail once and retained source tokens keep no task captures alive.
 * Failure hints: inspect deferred EntryHolder construction and its catch boundary.
 */
void control_connect_exception_notifies_dependants();
TEST(StdexecNative, ControlConnectExceptionNotifiesDependants) { control_connect_exception_notifies_dependants(); }

struct Observation
{
    std::atomic_uint m_count = 0;
    std::exception_ptr m_exception;
};

struct Receiver
{
    using receiver_concept = stde::receiver_t;
    struct Environment
    {
        stde::inplace_stop_token m_token;
        stde::inplace_stop_token query(stde::get_stop_token_t) const noexcept { return m_token; }
    };
    Environment get_env() const noexcept { return {m_token}; }
    void set_value() noexcept { ++m_observation_p->m_count; }
    void set_error(std::exception_ptr exception) noexcept
    {
        m_observation_p->m_exception = std::move(exception);
        ++m_observation_p->m_count;
    }
    stde::inplace_stop_token m_token;
    Observation * m_observation_p;
};

class ForwardScheduler
{
    using Self = ForwardScheduler;
public:
    using scheduler_concept = stde::scheduler_t;
    struct Sender
    {
        using sender_concept = stde::sender_t;
        using completion_signatures = stde::completion_signatures<stde::set_value_t()>;
        struct Environment
        {
            runtime::QueueScheduler m_scheduler;
            Self query(stde::get_completion_scheduler_t<stde::set_value_t>) const noexcept { return Self {m_scheduler}; }
        };
        Environment get_env() const noexcept { return {m_scheduler}; }
        template <typename Receiver>
        auto connect(Receiver receiver) const { return stde::connect(stde::schedule(m_scheduler), std::move(receiver)); }
        runtime::QueueScheduler m_scheduler;
    };
public:
    explicit ForwardScheduler(runtime::QueueScheduler scheduler) noexcept : m_scheduler(std::move(scheduler)) {}
    bool operator==(const Self &) const noexcept = default;
    Sender schedule() const noexcept { return {m_scheduler}; }
    template <typename Callback>
    void enqueue(Callback * callback_p) const noexcept { m_scheduler.enqueue(callback_p); }
    void workStarted() const noexcept { m_scheduler.workStarted(); }
    void workFinished() const noexcept { m_scheduler.workFinished(); }
private:
    runtime::QueueScheduler m_scheduler;
};

class FailingScheduler
{
    using Self = FailingScheduler;
public:
    using scheduler_concept = stde::scheduler_t;
    struct Sender
    {
        using sender_concept = stde::sender_t;
        using completion_signatures = stde::completion_signatures<stde::set_value_t(), stde::set_error_t(std::exception_ptr), stde::set_stopped_t()>;
        template <typename Receiver>
        struct Operation
        {
            using operation_state_concept = stde::operation_state_t;
            void start() noexcept
            {
                if (m_error) { stde::set_error(std::move(m_receiver), std::move(m_error)); }
                else { stde::set_stopped(std::move(m_receiver)); }
            }
            Receiver m_receiver;
            std::exception_ptr m_error;
        };
        struct Environment
        {
            runtime::QueueScheduler m_scheduler;
            std::exception_ptr m_error;
            bool m_throw_on_connect;
            Self query(stde::get_completion_scheduler_t<stde::set_value_t>) const noexcept { return Self {m_scheduler, m_error, m_throw_on_connect}; }
        };
        Environment get_env() const noexcept { return {m_scheduler, m_error, m_throw_on_connect}; }
        template <typename Receiver>
        auto connect(Receiver receiver) const
        {
            if (m_throw_on_connect) { throw std::system_error(std::make_error_code(std::errc::permission_denied)); }
            return Operation<Receiver> {std::move(receiver), m_error};
        }
        runtime::QueueScheduler m_scheduler;
        std::exception_ptr m_error;
        bool m_throw_on_connect;
    };
public:
    FailingScheduler(runtime::QueueScheduler scheduler, std::exception_ptr error, bool throw_on_connect = false) noexcept :
        m_scheduler(std::move(scheduler)), m_error(std::move(error)), m_throw_on_connect(throw_on_connect) {}
    bool operator==(const Self &) const noexcept = default;
    Sender schedule() const noexcept { return {m_scheduler, m_error, m_throw_on_connect}; }
    template <typename Callback>
    void enqueue(Callback * callback_p) const noexcept { m_scheduler.enqueue(callback_p); }
    void workStarted() const noexcept { m_scheduler.workStarted(); }
    void workFinished() const noexcept { m_scheduler.workFinished(); }
private:
    runtime::QueueScheduler m_scheduler;
    std::exception_ptr m_error;
    bool m_throw_on_connect;
};

void expect_cancelled(const Observation & observation)
{
    ASSERT_EQ(observation.m_count, 1u);
    ASSERT_TRUE(observation.m_exception);
    try { std::rethrow_exception(observation.m_exception); }
    catch (const std::system_error & error) { EXPECT_EQ(error.code(), std::errc::operation_canceled); }
    catch (...) { FAIL() << "Unexpected cancellation exception type"; }
}

void stopped_task_publishes_failure()
{
    lcf::async::testing::Runtime runtime;
    unsigned recovery_runs = 0;
    unsigned successor_runs = 0;
    auto stopped = task::Chain {runtime.cpu()}.link([]() -> lcf::async::Awaitable<int> {
        co_await stde::just_stopped();
        co_return 42;
    }, task::on_error([&](const task::ErrorHandleContext &) { ++recovery_runs; return task::use_value(42); }));
    const auto token = stopped.getResultToken();
    auto result_rh = runtime.run(std::move(stopped));
    ASSERT_FALSE(*result_rh);
    EXPECT_EQ(result_rh->error(), std::errc::operation_canceled);
    auto observed = runtime.run(task::merge(token).link(runtime.control(), [&](const int &) { ++successor_runs; }));
    ASSERT_FALSE(*observed);
    EXPECT_EQ(observed->error(), std::errc::operation_canceled);
    EXPECT_EQ(recovery_runs, 0u);
    EXPECT_EQ(successor_runs, 0u);
}

void pre_stopped_source_notifies_dependants()
{
    lcf::async::testing::Runtime runtime;
    std::stop_source stop_source;
    stop_source.request_stop();
    auto capture_sp = std::make_shared<int>(42);
    std::weak_ptr<int> capture_wp = capture_sp;
    unsigned user_runs = 0;
    auto source = task::Chain {runtime.cpu()}.link([capture_sp = std::move(capture_sp), &user_runs] { ++user_runs; return *capture_sp; });
    const auto token = source.getResultToken();
    auto tail = std::move(source).link([&](const int & value) { ++user_runs; return value + 1; });
    auto observer = task::merge(token).link(runtime.control(), [](const int &) { return 0; });
    unsigned source_completions = 0;
    unsigned observer_completions = 0;
    std::move(observer).start(runtime.control(), [&](const task::Result<int> & result) {
        ++observer_completions;
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error(), std::errc::operation_canceled);
    });
    std::move(tail).start(runtime.control(), runtime::bind_stop_token(stop_source.get_token(), [&](const task::Result<int> & result) {
        ++source_completions;
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error(), std::errc::operation_canceled);
    }));
    runtime.pumpUntil([&] { return source_completions != 0u and observer_completions != 0u; });
    EXPECT_EQ(source_completions, 1u);
    EXPECT_EQ(observer_completions, 1u);
    EXPECT_EQ(user_runs, 0u);
    EXPECT_TRUE(capture_wp.expired());
}

void pre_stopped_timer_completes()
{
    stde::inplace_stop_source stop_source;
    stop_source.request_stop();
    Observation observation;
    auto operation = stde::connect(runtime::delay(std::chrono::hours {1}), Receiver {stop_source.get_token(), &observation});
    stde::start(operation);
    expect_cancelled(observation);
}

void timer_start_races_stop()
{
    for (unsigned index = 0; index < 64; ++index) {
        SCOPED_TRACE(index);
        stde::inplace_stop_source stop_source;
        Observation observation;
        auto operation = stde::connect(runtime::delay(std::chrono::hours {1}), Receiver {stop_source.get_token(), &observation});
        std::jthread stopper {[&] { stop_source.request_stop(); }};
        stde::start(operation);
        stopper.join();
        expect_cancelled(observation);
    }
}

void notification_races_stop()
{
    for (unsigned index = 0; index < 64; ++index) {
        SCOPED_TRACE(index);
        runtime::Context context;
        runtime::Signal direct_signal;
        runtime::Signal queued_signal {context.getExecutor()};
        auto & signal = index % 2u == 0u ? direct_signal : queued_signal;
        stde::inplace_stop_source stop_source;
        Observation observation;
        auto operation = stde::connect(signal.wait(), Receiver {stop_source.get_token(), &observation});
        stde::start(operation);
        std::jthread producer {[&] { signal.set(); }};
        std::jthread stopper {[&] { stop_source.request_stop(); }};
        producer.join();
        stopper.join();
        context.poll();
        ASSERT_EQ(observation.m_count, 1u);
        if (observation.m_exception) { expect_cancelled(observation); }
    }
}

void ready_nodes_keep_queue_boundaries()
{
    runtime::Context context;
    const auto executor = context.getExecutor();
    unsigned user_runs = 0;
    auto chain = task::Chain {0u, executor};
    for (unsigned index = 0; index < 128; ++index) {
        chain = std::move(chain).link([&](const unsigned & value) { ++user_runs; return value + 1; });
    }
    bool done = false;
    std::move(chain).start(executor, [&](const task::Result<unsigned> & result) {
        done = true;
        ASSERT_TRUE(result);
        EXPECT_EQ(result.value(), 128u);
    });
    EXPECT_EQ(user_runs, 0u);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds {10};
    while (not done) {
        const auto before = user_runs;
        context.pollOne();
        ASSERT_LE(user_runs - before, 1u);
        ASSERT_LT(std::chrono::steady_clock::now(), deadline);
    }
    EXPECT_EQ(user_runs, 128u);
}

void executor_accepts_distinct_callback_schedulers()
{
    static_assert(stde::scheduler<ForwardScheduler>);
    runtime::Context context;
    lcf::async::Executor adapted {ForwardScheduler {context.getScheduler()}};
    auto chain = task::Chain {40, context.getExecutor()}
        .link(adapted, [](const int & value) { return value + 1; })
        .link(context.getExecutor(), [](const int & value) { return value + 1; });
    unsigned completions = 0;
    std::move(chain).start(adapted, [&](const task::Result<int> & result) {
        ++completions;
        EXPECT_TRUE(result);
        if (result) { EXPECT_EQ(result.value(), 42); }
        throw std::runtime_error("ordinary completion");
    });
    EXPECT_THROW(context.runUntilIdle(), std::runtime_error);
    EXPECT_EQ(completions, 1u);
}

void check_scheduler_failure(std::exception_ptr exception, std::error_code expected)
{
    static_assert(stde::scheduler<FailingScheduler>);
    runtime::Context context;
    lcf::async::Executor executor {FailingScheduler {context.getScheduler(), std::move(exception)}};
    unsigned task_runs = 0;
    unsigned handler_runs = 0;
    unsigned completions = 0;
    auto chain = task::Chain {executor}.link([&] { ++task_runs; return 42; },
        task::on_error([&](const task::ErrorHandleContext &) { ++handler_runs; return task::use_value(42); }));
    std::move(chain).start(context.getExecutor(), [&](const task::Result<int> & result) {
        ++completions;
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error(), expected);
    });
    context.runUntilIdle();
    EXPECT_EQ(task_runs, 0u);
    EXPECT_EQ(handler_runs, 0u);
    EXPECT_EQ(completions, 1u);
}

void executor_preserves_scheduler_stopped()
{
    check_scheduler_failure({}, std::make_error_code(std::errc::operation_canceled));
}

void executor_preserves_scheduler_error()
{
    const auto error = std::make_error_code(std::errc::permission_denied);
    check_scheduler_failure(std::make_exception_ptr(std::system_error(error)), error);
}

void check_child_scheduler(bool fail)
{
    lcf::async::testing::Runtime runtime;
    const auto control_thread = std::this_thread::get_id();
    std::thread::id entry_thread;
    std::thread::id worker_thread;
    std::thread::id continuation_thread;
    std::thread::id yield_thread;
    auto child = [&]() -> runtime::Awaitable<int> {
        entry_thread = std::this_thread::get_id();
        co_await exec::reschedule_coroutine_on(runtime.cpu());
        worker_thread = std::this_thread::get_id();
        if (fail) { throw std::system_error(std::make_error_code(std::errc::permission_denied)); }
        co_return 42;
    };
    auto chain = task::Chain {runtime.control()}.link([&]() -> runtime::Awaitable<int> {
        int value = 0;
        std::exception_ptr exception;
        try { value = co_await child(); }
        catch (...) { exception = std::current_exception(); }
        continuation_thread = std::this_thread::get_id();
        co_await runtime::yield();
        yield_thread = std::this_thread::get_id();
        if (exception) { std::rethrow_exception(std::move(exception)); }
        co_return value;
    });
    const auto result_rh = runtime.run(std::move(chain));
    EXPECT_EQ(entry_thread, control_thread);
    EXPECT_NE(worker_thread, control_thread);
    EXPECT_EQ(continuation_thread, control_thread);
    EXPECT_EQ(yield_thread, control_thread);
    if (fail) {
        ASSERT_FALSE(*result_rh);
        EXPECT_EQ(result_rh->error(), std::errc::permission_denied);
    } else {
        ASSERT_TRUE(*result_rh);
        EXPECT_EQ(result_rh->value(), 42);
    }
}

void child_task_restores_parent_scheduler() { check_child_scheduler(false); }
void failed_child_restores_parent_scheduler() { check_child_scheduler(true); }

void awaitable_works_with_sync_wait()
{
    const auto make_value = []() -> runtime::Awaitable<std::unique_ptr<int>> {
        co_return std::make_unique<int>(42);
    };
    auto result = stde::sync_wait(make_value());
    ASSERT_TRUE(result);
    const auto & value_up = std::get<0>(*result);
    ASSERT_TRUE(value_up);
    EXPECT_EQ(*value_up, 42);
}

void check_control_failure(std::exception_ptr exception, bool throw_on_connect, std::error_code expected)
{
    runtime::Context context;
    const auto executor = context.getExecutor();
    runtime::Executor controller {FailingScheduler {context.getScheduler(), std::move(exception), throw_on_connect}};
    unsigned task_runs = 0;
    unsigned branch_runs = 0;
    unsigned source_completions = 0;
    unsigned observer_completions = 0;
    auto capture_sp = std::make_shared<int>(42);
    const std::weak_ptr<int> capture_wp = capture_sp;
    auto source = task::Chain {executor}.link([capture_sp = std::move(capture_sp), &task_runs] { ++task_runs; return *capture_sp; });
    const auto token = source.getResultToken();
    auto observer = task::merge(token).link(executor, [&](const int &) { ++branch_runs; });
    std::move(observer).start(executor, [&](const task::Result<void> & result) {
        ++observer_completions;
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error(), expected);
    });
    std::move(source).start(controller, [&](const task::Result<int> & result) {
        ++source_completions;
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error(), expected);
    });
    context.runUntilIdle();
    EXPECT_EQ(source_completions, 1u);
    EXPECT_EQ(observer_completions, 1u);
    EXPECT_EQ(task_runs, 0u);
    EXPECT_EQ(branch_runs, 0u);
    EXPECT_TRUE(capture_wp.expired());
}

void control_scheduler_error_notifies_dependants()
{
    const auto error = std::make_error_code(std::errc::permission_denied);
    check_control_failure(std::make_exception_ptr(std::system_error(error)), false, error);
}

void stopped_control_scheduler_notifies_dependants()
{
    check_control_failure({}, false, std::make_error_code(std::errc::operation_canceled));
}

void control_connect_exception_notifies_dependants()
{
    check_control_failure({}, true, std::make_error_code(std::errc::permission_denied));
}

}
