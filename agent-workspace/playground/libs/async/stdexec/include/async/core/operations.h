#pragma once

#include <async/dependencies.h>

#if defined(LCF_ASYNC_USE_MODULES) and LCF_ASYNC_USE_MODULES \
    and not defined(LCF_ASYNC_BUILDING_MODULE) \
    and not defined(LCF_ASYNC_FORCE_HEADERS) \
    and not defined(__INTELLISENSE__)
import lcf.async;
#else
#include <async/execution/coroutine/operations.h>
#include "Context.h"
#include "Awaitable.h"
#include "timer.h"

namespace lcf::async {

LCF_ASYNC_EXPORT
template <typename Function>
void post(Executor executor, Function function) { executor.post(std::move(function)); }

LCF_ASYNC_EXPORT
using execution::yield;

}
#endif
