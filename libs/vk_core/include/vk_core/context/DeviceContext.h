#pragma once
#include <vulkan/vulkan.hpp>
#include <memory>
#include <vector>
#include <expected>
#include "vk_core/context/enums.h"
#include "vk_core/memory/MemoryAllocator.h"
#include "vk_core/queue/LogicalQueue.h"

namespace lcf::vkc::details {

class DeviceQueue;

}

namespace lcf::vkc {

class DeviceContextCreateInfo;

class DeviceContext
{
    using Self = DeviceContext;
public:
    ~DeviceContext() noexcept;
    DeviceContext() noexcept;
    DeviceContext(const Self &) = delete;
    DeviceContext(Self &&) = default;
    Self & operator =(const Self &) = delete;
    Self & operator =(Self &&) = default;
public:
    std::error_code create(vk::Instance instance, const DeviceContextCreateInfo & create_info) noexcept;
    const vk::PhysicalDevice & getPhysicalDevice() const noexcept { return m_physical_device; }
    const vk::Device & getDevice() const noexcept { return m_device.get(); }
    const MemoryAllocator & getMemoryAllocator() const noexcept { return m_memory_allocator; }
    const LogicalQueue & getLogicalQueue(const QueueKey & key) const noexcept { return m_logical_queues[std::to_underlying(key)]; }
private:
    vk::PhysicalDevice m_physical_device;
    vk::UniqueDevice m_device;
    MemoryAllocator m_memory_allocator;
    std::vector<std::unique_ptr<details::DeviceQueue>> m_device_queues;
    std::vector<LogicalQueue> m_logical_queues;
};


} // namespace lcf::vkc