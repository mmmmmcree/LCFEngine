#include "vk_core/context/QueueContext.h"

namespace lcf::vkc { 

std::error_code QueueContext::create(vk::Device device, uint32_t family_index, uint32_t queue_index) noexcept
{
    m_family_index = family_index;
    m_queue = device.getQueue(family_index, queue_index);
    return m_timeline.create(device);
}

} // namespace lcf::vkc