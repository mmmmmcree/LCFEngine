#pragma once

#if defined(LCF_ASYNC_USE_MODULES) and LCF_ASYNC_USE_MODULES \
    and not defined(LCF_ASYNC_FORCE_HEADERS) \
    and not defined(__INTELLISENSE__)
import lcf.async;
#else
#include "core/Awaitable.h"
#include "core/Executor.h"
#include "core/Context.h"
#include "core/Signal.h"
#include "core/operations.h"
#include "core/cancellation.h"
#endif
