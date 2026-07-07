#pragma once

#include <vulkan/vulkan.hpp>
#include <memory>
#include <vector>
#include <expected>
#include "vk_core/memory/MemoryAllocator.h"

namespace lcf::vkc {

class DeviceContextCreateInfo;

class QueueContext;

class RenderDeviceContext
{
    using Self = RenderDeviceContext;
    using QueueContextUP = std::unique_ptr<QueueContext>;
public:
    ~RenderDeviceContext() noexcept;
    RenderDeviceContext() = default;
    RenderDeviceContext(const Self &) = delete;
    RenderDeviceContext(Self &&) = default;
    Self & operator =(const Self &) = delete;
    Self & operator =(Self &&) = default;
public:
    std::error_code create(vk::Instance instance, const DeviceContextCreateInfo & create_info) noexcept;
    const vk::PhysicalDevice & getPhysicalDevice() const noexcept { return m_physical_device; }
    const vk::Device & getDevice() const noexcept { return m_device.get(); }
    const MemoryAllocator & getMemoryAllocator() const noexcept { return m_memory_context; }
    QueueContext & getGraphicsQueueContext() noexcept { return *m_graphics_queue_context_p; }
    const QueueContext & getGraphicsQueueContext() const noexcept { return *m_graphics_queue_context_p; }
    QueueContext & getComputeQueueContext() noexcept { return *m_compute_queue_context_p; }
    const QueueContext & getComputeQueueContext() const noexcept { return *m_compute_queue_context_p; }
    QueueContext & getTransferQueueContext() noexcept { return *m_transfer_queue_context_p; }
    const QueueContext & getTransferQueueContext() const noexcept { return *m_transfer_queue_context_p; }
private:
    vk::PhysicalDevice m_physical_device;
    vk::UniqueDevice m_device;
    MemoryAllocator m_memory_context;
    std::vector<QueueContextUP> m_queue_contexts;
    QueueContext * m_graphics_queue_context_p = nullptr;
    QueueContext * m_compute_queue_context_p = nullptr;
    QueueContext * m_transfer_queue_context_p = nullptr;
};


} // namespace lcf::vkc