#include "vk_core/context/QueueContext.h"

namespace lcf::vkc { 

std::error_code QueueContext::create(vk::Device device, uint32_t family_index, uint32_t queue_index) noexcept
{
    m_family_index = family_index;
    m_queue = device.getQueue(family_index, queue_index);
    if (auto ec = m_cmd_allocator.create(device, m_family_index, this)) { return ec; }
    if (auto ec = m_timeline.create(device)) { return ec; }
    return {}; 
}


void QueueContext::submit(CommandBufferProxy && cmd_proxy) noexcept
{

}


void QueueContext::collectGarbage() noexcept
{
    
}

} // namespace lcf::vkc