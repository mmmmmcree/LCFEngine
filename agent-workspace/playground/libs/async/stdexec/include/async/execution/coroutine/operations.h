#pragma once

#include <async/dependencies.h>

#if defined(LCF_ASYNC_USE_MODULES) and LCF_ASYNC_USE_MODULES \
    and not defined(LCF_ASYNC_BUILDING_MODULE) \
    and not defined(LCF_ASYNC_FORCE_HEADERS) \
    and not defined(__INTELLISENSE__)
import lcf.async;
#else
#include "Awaitable.h"

namespace lcf::async::execution {

namespace details {

class YieldAwaitable
{
    struct Environment
    {
        static constexpr auto query(exec::get_completion_behavior_t<stde::set_value_t>) noexcept
        {
            return exec::completion_behavior::asynchronous_affine;
        }
    };
public:
    using sender_concept = stde::sender_t;
    using completion_signatures = stde::completion_signatures<stde::set_value_t(), stde::set_error_t(std::exception_ptr), stde::set_stopped_t()>;
public:
    Environment get_env() const noexcept { return {}; }
    template <typename Promise>
    requires requires (Promise & promise) { stde::get_start_scheduler(stde::get_env(promise)); }
    auto as_awaitable(Promise & promise) const
    {
        return stde::as_awaitable(stde::schedule(stde::get_start_scheduler(stde::get_env(promise))), promise);
    }
};

}

LCF_ASYNC_EXPORT
inline Awaitable<void> yield()
{
    co_await details::YieldAwaitable {};
}

}
#endif
