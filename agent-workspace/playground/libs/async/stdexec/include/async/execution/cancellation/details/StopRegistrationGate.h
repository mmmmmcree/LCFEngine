#pragma once

#include <atomic>

namespace lcf::async::execution::details {

// A pre-stopped token can invoke Cancel before the callback constructor returns.
class StopRegistrationGate
{
    enum class Phase { eRegistering, eReady, eCancelled };
public:
    bool requestCancellation() noexcept
    {
        return m_phase.exchange(Phase::eCancelled, std::memory_order_acq_rel) == Phase::eReady;
    }
    bool finishRegistration() noexcept
    {
        return m_phase.exchange(Phase::eReady, std::memory_order_acq_rel) == Phase::eCancelled;
    }
private:
    std::atomic<Phase> m_phase {Phase::eRegistering};
};

}
