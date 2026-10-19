#pragma once

#include <async/execution/stdexec.h>
#include "timer.h"
#include <async/execution/cancellation/details/StopRegistrationGate.h>

namespace lcf::async::execution {

namespace details {

template <typename Receiver>
class TimerRegistration : public TimerEntry, public std::enable_shared_from_this<TimerRegistration<Receiver>>
{
    using Self = TimerRegistration;
    using StopToken = stde::stop_token_of_t<stde::env_of_t<Receiver>>;
    struct Cancel
    {
        std::weak_ptr<Self> m_registration_wp;
        void operator()() const noexcept
        {
            if (auto registration_sp = m_registration_wp.lock(); registration_sp and registration_sp->m_stop_gate.requestCancellation()) {
                TimerService::instance().cancel(std::move(registration_sp));
            }
        }
    };
    using StopCallback = stde::stop_callback_for_t<StopToken, Cancel>;
public:
    TimerRegistration(Receiver receiver, std::chrono::steady_clock::duration delay) :
        TimerEntry {std::chrono::steady_clock::now() + delay, &Self::finish}, m_receiver(std::move(receiver)),
        m_cancel_error(std::make_exception_ptr(std::system_error(std::make_error_code(std::errc::operation_canceled))))
    {
        TimerService::instance();
    }
    void start() noexcept
    {
        auto self_sp = this->shared_from_this();
        m_stop_callback.emplace(stde::get_stop_token(stde::get_env(m_receiver)), Cancel {self_sp});
        if (m_stop_gate.finishRegistration()) { TimerService::instance().cancel(std::move(self_sp)); return; }
        TimerService::instance().submit(std::move(self_sp));
    }
private:
    static void finish(std::shared_ptr<TimerEntry> entry_sp, bool cancelled) noexcept
    {
        auto & registration = *static_cast<Self *>(entry_sp.get());
        registration.m_stop_callback.reset();
        if (cancelled) { stde::set_error(std::move(registration.m_receiver), registration.m_cancel_error); }
        else { stde::set_value(std::move(registration.m_receiver)); }
    }
private:
    Receiver m_receiver;
    std::exception_ptr m_cancel_error;
    std::optional<StopCallback> m_stop_callback;
    StopRegistrationGate m_stop_gate;
};

}

}
