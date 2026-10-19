#pragma once

#if defined(LCF_ASYNC_USE_MODULES) and LCF_ASYNC_USE_MODULES \
    and not defined(LCF_ASYNC_FORCE_HEADERS) \
    and not defined(__INTELLISENSE__)
import lcf.async;
#else
#include <asio/bind_cancellation_slot.hpp>
#include <asio/cancellation_signal.hpp>
#include <utility>

namespace lcf::async {

using CancellationSource = asio::cancellation_signal;

template <typename Completion>
auto bind_cancellation(CancellationSource & source, Completion completion)
{
    return asio::bind_cancellation_slot(source.slot(), std::move(completion));
}

inline void request_cancellation(CancellationSource & source) { source.emit(asio::cancellation_type::terminal); }

}
#endif
