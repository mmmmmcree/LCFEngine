#include <async/execution.h>
#include <async/core/Signal.h>
#include <async/core/timer.h>
#include <gtest/gtest.h>

namespace {

namespace runtime = lcf::async::execution;
namespace stde = stdexec;

struct StopCase
{
    const char * m_name;
    bool m_timer;
    bool m_pre_stopped;
};

class StopRegistrationTest : public ::testing::TestWithParam<StopCase> {};

/*
 * Objective: terminal receivers may immediately destroy their associated stop source.
 * Input: notification/timer waits with active or pre-stopped tokens, selected by case name.
 * Procedure: give the receiver sole source ownership and release it inside set_value/set_error.
 * Expected: one terminal call, source expiration before completion, and cancellation only for pre-stop.
 * Failure hints: inspect callback unregistration before terminal delivery and StopRegistrationGate.
 */
void receiver_destroys_stop_source(const StopCase & test_case);
TEST_P(StopRegistrationTest, ReceiverCanDestroyStopSource) { receiver_destroys_stop_source(GetParam()); }

const StopCase k_stop_cases[] {
    {"NotificationReady", false, false}, {"NotificationPreStopped", false, true},
    {"TimerReady", true, false}, {"TimerPreStopped", true, true}};
INSTANTIATE_TEST_SUITE_P(WaitKinds, StopRegistrationTest, ::testing::ValuesIn(k_stop_cases),
    [](const auto & parameter) { return parameter.param.m_name; });

struct Observation
{
    std::atomic_uint m_count {0};
    std::exception_ptr m_exception;
};

struct Receiver
{
    using receiver_concept = stde::receiver_t;
    auto get_env() const noexcept { return stde::prop {stde::get_stop_token, m_source_sp->get_token()}; }
    void set_value() noexcept { this->complete(); }
    void set_error(std::exception_ptr exception) noexcept { m_observation_p->m_exception = std::move(exception); this->complete(); }
    void complete() noexcept
    {
        m_source_sp.reset();
        m_observation_p->m_count.fetch_add(1, std::memory_order_release);
    }
    std::shared_ptr<stde::inplace_stop_source> m_source_sp;
    Observation * m_observation_p;
};

void receiver_destroys_stop_source(const StopCase & test_case)
{
    auto source_sp = std::make_shared<stde::inplace_stop_source>();
    const std::weak_ptr<stde::inplace_stop_source> source_wp = source_sp;
    if (test_case.m_pre_stopped) { source_sp->request_stop(); }
    Observation observation;
    const auto await_completion = [&] {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds {2};
        while (observation.m_count.load(std::memory_order_acquire) == 0 and std::chrono::steady_clock::now() < deadline) {
            std::this_thread::yield();
        }
        ASSERT_EQ(observation.m_count.load(std::memory_order_acquire), 1u);
        EXPECT_TRUE(source_wp.expired());
        if (not test_case.m_pre_stopped) { EXPECT_FALSE(observation.m_exception); return; }
        ASSERT_TRUE(observation.m_exception);
        try { std::rethrow_exception(observation.m_exception); }
        catch (const std::system_error & error) { EXPECT_EQ(error.code(), std::errc::operation_canceled); }
        catch (...) { FAIL() << "Unexpected cancellation exception type"; }
    };
    if (test_case.m_timer) {
        const auto delay = test_case.m_pre_stopped ? std::chrono::hours {1} : std::chrono::hours {0};
        auto operation = stde::connect(runtime::delay(delay), Receiver {std::move(source_sp), &observation});
        stde::start(operation);
        await_completion();
    } else {
        runtime::Signal signal;
        auto operation = stde::connect(signal.wait(), Receiver {std::move(source_sp), &observation});
        stde::start(operation);
        if (not test_case.m_pre_stopped) { EXPECT_TRUE(signal.set()); }
        await_completion();
    }
}

}
