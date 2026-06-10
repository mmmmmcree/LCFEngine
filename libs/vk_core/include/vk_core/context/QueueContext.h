#pragma once

#include "vk_core/sync/TimelineSemaphore.h"
#include <vulkan/vulkan.hpp>

namespace lcf::vkc {

class QueueContext
{
public:

private:
    vk::Queue m_queue;
    TimelineSemaphore m_timeline;
};

} // namespace lcf::vkc