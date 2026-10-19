#pragma once

#include <async/dependencies.h>

#if defined(LCF_ASYNC_USE_MODULES) and LCF_ASYNC_USE_MODULES \
    and not defined(LCF_ASYNC_BUILDING_MODULE) \
    and not defined(LCF_ASYNC_FORCE_HEADERS) \
    and not defined(__INTELLISENSE__)
import lcf.async;
#else
#include "details/timer_wait.h"

namespace lcf::async::execution {

LCF_ASYNC_EXPORT
struct DelaySender
{
    using sender_concept = stde::sender_t;
    using completion_signatures = stde::completion_signatures<stde::set_value_t(), stde::set_error_t(std::exception_ptr)>;
    template <typename Receiver>
    struct Operation
    {
        using operation_state_concept = stde::operation_state_t;
        void start() noexcept { auto registration_sp = std::move(m_registration_sp); registration_sp->start(); }
        std::shared_ptr<details::TimerRegistration<Receiver>> m_registration_sp;
    };
    template <stde::receiver Receiver>
    auto connect(Receiver receiver) const
    {
        using Registration = details::TimerRegistration<Receiver>;
        return Operation<Receiver> {std::allocate_shared<Registration>(std::pmr::polymorphic_allocator<Registration> {m_resource_p},
            std::move(receiver), m_delay)};
    }
    std::chrono::steady_clock::duration m_delay;
    std::pmr::memory_resource * m_resource_p;
};

LCF_ASYNC_EXPORT
inline DelaySender delay(std::chrono::steady_clock::duration duration,
    std::pmr::memory_resource * resource_p = std::pmr::get_default_resource()) noexcept { return {duration, resource_p}; }

}
#endif
