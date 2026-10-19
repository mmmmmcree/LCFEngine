#pragma once

#if defined(LCF_ASYNC_USE_MODULES) and LCF_ASYNC_USE_MODULES \
    and not defined(LCF_ASYNC_FORCE_HEADERS) \
    and not defined(__INTELLISENSE__)
import lcf.async;
#else
#include "Awaitable.h"
#include "Executor.h"
#include "timer.h"
#include <asio/post.hpp>
#include <asio/use_awaitable.hpp>
#include <utility>

namespace lcf::async {

template <typename Function>
void post(Executor executor, Function function) { asio::post(std::move(executor), std::move(function)); }

inline Awaitable<void> yield() { co_await asio::post(asio::use_awaitable); }

}
#endif
