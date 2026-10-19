#pragma once

#include <async/dependencies.h>

#if defined(LCF_ASYNC_USE_MODULES) and LCF_ASYNC_USE_MODULES \
    and not defined(LCF_ASYNC_BUILDING_MODULE) \
    and not defined(LCF_ASYNC_FORCE_HEADERS) \
    and not defined(__INTELLISENSE__)
import lcf.async;
#else
#include <async/execution/cancellation/operations.h>

namespace lcf::async {

LCF_ASYNC_EXPORT
using CancellationSource = std::stop_source;

LCF_ASYNC_EXPORT
template <typename Completion>
auto bind_cancellation(CancellationSource & source, Completion completion)
{
    return execution::bind_stop_token(source.get_token(), std::move(completion));
}

LCF_ASYNC_EXPORT
inline void request_cancellation(CancellationSource & source) noexcept { source.request_stop(); }

}
#endif
