#include "vk_core/queue/QueueAccess.h"
#include "vk_core/queue/LogicalQueue.h"
#include "vk_core/queue/details/DeviceQueue.h"

namespace lcf::vkc {

QueueAccess::QueueAccess(const LogicalQueue &logical_queue) noexcept :
    m_queue(logical_queue.m_device_queue_p->getQueue()),
    m_lock([&]() -> UniqueLock {
        auto & mutex_opt = logical_queue.m_device_queue_p->getMutexOpt();
        return mutex_opt ? UniqueLock(*mutex_opt) : UniqueLock{};
    }())
{
}

} // namespace lcf::vkc

