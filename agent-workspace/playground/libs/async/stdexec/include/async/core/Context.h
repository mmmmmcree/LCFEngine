#pragma once

#include <async/dependencies.h>

#if defined(LCF_ASYNC_USE_MODULES) and LCF_ASYNC_USE_MODULES \
    and not defined(LCF_ASYNC_BUILDING_MODULE) \
    and not defined(LCF_ASYNC_FORCE_HEADERS) \
    and not defined(__INTELLISENSE__)
import lcf.async;
#else
#include <async/execution/scheduling/Context.h>
#include "Executor.h"

namespace lcf::async {

LCF_ASYNC_EXPORT
using Context = execution::Context;

}
#endif
