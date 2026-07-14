#include "vk_core/queue/LogicalQueue.h"
#include "vk_core/queue/details/DeviceQueue.h"

namespace lcf::vkc {

const vk::Device & LogicalQueue::getDevice() const noexcept
{
    return m_device_queue_p->getDevice();
}

const uint32_t & LogicalQueue::getFamilyIndex() const noexcept
{
    return m_device_queue_p->getFamilyIndex();
}

// QueueAccess LogicalQueue::acquireAccess() const noexcept
// {
//     return QueueAccess(m_device_queue_p->getQueue(), m_device_queue_p->getMutexOpt());
// }

} // namespace lcf::vkc

