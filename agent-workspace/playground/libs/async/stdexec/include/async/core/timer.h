#pragma once

#include <async/dependencies.h>

#if defined(LCF_ASYNC_USE_MODULES) and LCF_ASYNC_USE_MODULES \
    and not defined(LCF_ASYNC_BUILDING_MODULE) \
    and not defined(LCF_ASYNC_FORCE_HEADERS) \
    and not defined(__INTELLISENSE__)
import lcf.async;
#else
#include <async/execution/timer/operations.h>
#include "Context.h"
#include "Awaitable.h"

namespace lcf::async {

LCF_ASYNC_EXPORT
inline auto delay(std::chrono::steady_clock::duration duration) noexcept { return execution::delay(duration); }

}
#endif
