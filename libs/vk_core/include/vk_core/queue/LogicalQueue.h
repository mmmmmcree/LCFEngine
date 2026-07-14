#pragma once

#include <vulkan/vulkan.hpp>
#include "vk_core/queue/QueueAccess.h"

namespace lcf::vkc::details {

class DeviceQueue;

class QueueAccess;

}

namespace lcf::vkc {

class LogicalQueue
{
    using Self = LogicalQueue;
    friend class QueueAccess;
public:
    ~LogicalQueue() noexcept = default;
    LogicalQueue() noexcept = default;
    explicit LogicalQueue(const details::DeviceQueue & device_queue) noexcept : m_device_queue_p(&device_queue) {}
    LogicalQueue(const Self &) = default;
    LogicalQueue(Self &&) = default;
    Self &operator=(const Self &) = default;
    Self &operator=(Self &&) = default;
public:
    const vk::Device & getDevice() const noexcept;
    const uint32_t & getFamilyIndex() const noexcept;
private:
    const details::DeviceQueue * m_device_queue_p = nullptr;
};

} // namespace lcf::vkc