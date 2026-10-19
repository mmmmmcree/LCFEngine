#pragma once

#include <async/dependencies.h>

#if defined(LCF_ASYNC_USE_MODULES) and LCF_ASYNC_USE_MODULES \
    and not defined(LCF_ASYNC_BUILDING_MODULE) \
    and not defined(LCF_ASYNC_FORCE_HEADERS) \
    and not defined(__INTELLISENSE__)
import lcf.async;
#else
#include <async/execution/stdexec.h>
#include "details/frame.h"

namespace lcf::async::execution {

// Selects a resource for synchronous coroutine creation on this thread, before suspension.
// The resource must outlive those frames and support their eventual deallocation threads.
LCF_ASYNC_EXPORT
class FrameAllocatorScope
{
    using Self = FrameAllocatorScope;
public:
    ~FrameAllocatorScope() noexcept { details::current_frame_resource() = m_previous_p; }
    explicit FrameAllocatorScope(std::pmr::memory_resource & resource) noexcept :
        m_previous_p(std::exchange(details::current_frame_resource(), &resource)) {}
    FrameAllocatorScope(const Self &) = delete;
    FrameAllocatorScope(Self &&) = delete;
    Self & operator=(const Self &) = delete;
    Self & operator=(Self &&) = delete;
private:
    std::pmr::memory_resource * m_previous_p;
};

}
#endif
