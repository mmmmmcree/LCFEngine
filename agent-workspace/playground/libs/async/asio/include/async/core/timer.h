#pragma once

#if defined(LCF_ASYNC_USE_MODULES) and LCF_ASYNC_USE_MODULES \
    and not defined(LCF_ASYNC_FORCE_HEADERS) \
    and not defined(__INTELLISENSE__)
import lcf.async;
#else
#include "Awaitable.h"
#include <asio/steady_timer.hpp>
#include <asio/this_coro.hpp>
#include <asio/use_awaitable.hpp>
#include <chrono>

namespace lcf::async {

inline Awaitable<void> delay(std::chrono::steady_clock::duration duration)
{
    asio::steady_timer timer {co_await asio::this_coro::executor};
    timer.expires_after(duration);
    co_await timer.async_wait(asio::use_awaitable);
}

}
#endif
