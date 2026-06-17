#pragma once

#include <vulkan/vulkan.hpp>
#include "vk_core/sync/TimelineSemaphore.h"

namespace lcf::vkc {

class QueueContext
{
    using Self = QueueContext;
public:
    ~QueueContext() noexcept = default;
    QueueContext() = default;
    QueueContext(const Self &) = delete;
    QueueContext(Self &&) = default;
    Self & operator=(const Self &) = delete;
    Self & operator=(Self &&) = default;
public:
    std::error_code create(vk::Device device, uint32_t family_index, uint32_t queue_index) noexcept;
private:
    uint32_t m_family_index = 0;
    vk::Queue m_queue;
    TimelineSemaphore m_timeline;
};

} // namespace lcf::vkc