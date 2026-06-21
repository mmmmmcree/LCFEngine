#pragma once

#include <vulkan/vulkan.hpp>
#include "vk_core/sync/TimelineSemaphore.h"
#include "resource_utils.h"

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
    const vk::Queue & getQueue() const noexcept { return m_queue; }
    const uint32_t & getFamilyIndex() const noexcept { return m_family_index; }
    void acquireResourceLease(ResourceLease lease) noexcept;
private:
    vk::Queue m_queue;
    uint32_t m_family_index;
    TimelineSemaphore m_timeline;
};

} // namespace lcf::vkc