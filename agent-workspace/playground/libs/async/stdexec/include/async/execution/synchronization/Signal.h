#pragma once

#include <async/dependencies.h>

#if defined(LCF_ASYNC_USE_MODULES) and LCF_ASYNC_USE_MODULES \
    and not defined(LCF_ASYNC_BUILDING_MODULE) \
    and not defined(LCF_ASYNC_FORCE_HEADERS) \
    and not defined(__INTELLISENSE__)
import lcf.async;
#else
#include "details/notification.h"

namespace lcf::async::execution {

LCF_ASYNC_EXPORT
class Signal
{
    using Self = Signal;
public:
    explicit Signal(std::pmr::memory_resource * resource_p = std::pmr::get_default_resource()) :
        m_notification_sp(std::allocate_shared<details::Notification>(
            std::pmr::polymorphic_allocator<details::Notification> {resource_p}, 1, resource_p)),
        m_resource_p(resource_p) {}
    Signal(const Self &) = delete;
    explicit Signal(Executor executor, std::pmr::memory_resource * resource_p = std::pmr::get_default_resource()) :
        Signal(resource_p) { m_executor_opt = std::move(executor); }
    Self & operator=(const Self &) = delete;
public:
    bool set() noexcept
    {
        if (m_set.exchange(true, std::memory_order_acq_rel)) { return false; }
        m_notification_sp->complete();
        return true;
    }
    auto wait() const noexcept { return details::NotificationSender {m_notification_sp, m_resource_p, m_executor_opt}; }
private:
    std::shared_ptr<details::Notification> m_notification_sp;
    std::pmr::memory_resource * m_resource_p;
    std::atomic_bool m_set = false;
    std::optional<Executor> m_executor_opt;
};

}
#endif
