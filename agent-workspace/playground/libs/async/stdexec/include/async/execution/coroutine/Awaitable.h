#pragma once

#include <async/dependencies.h>

#if defined(LCF_ASYNC_USE_MODULES) and LCF_ASYNC_USE_MODULES \
    and not defined(LCF_ASYNC_BUILDING_MODULE) \
    and not defined(LCF_ASYNC_FORCE_HEADERS) \
    and not defined(__INTELLISENSE__)
import lcf.async;
#else
#include "details/traits.h"

namespace lcf::async::execution {

LCF_ASYNC_EXPORT
template <typename T = void>
class [[nodiscard]] Awaitable
{
    using Self = Awaitable;
    using Handle = std::coroutine_handle<details::AwaitablePromise<T>>;
public:
    using promise_type = details::AwaitablePromise<T>;
    using sender_concept = stde::sender_t;
public:
    ~Awaitable() noexcept { if (m_handle) { m_handle.destroy(); } }
    explicit Awaitable(Handle handle) noexcept : m_handle(handle) {}
    Awaitable(const Self &) = delete;
    Awaitable(Self && other) noexcept : m_handle(std::exchange(other.m_handle, {})) {}
    Self & operator=(const Self &) = delete;
    Self & operator=(Self &&) = delete;
public:
    details::TaskAttributes get_env() const noexcept { return {}; }
    template <typename ParentPromise>
    requires requires (ParentPromise & promise) { stde::get_start_scheduler(stde::get_env(promise)); }
    auto as_awaitable(ParentPromise &) && noexcept
    {
        return details::TaskAwaiter<T, ParentPromise> {std::exchange(m_handle, {})};
    }
private:
    Handle m_handle;
};

}

#include "details/awaitable.h"
#endif
