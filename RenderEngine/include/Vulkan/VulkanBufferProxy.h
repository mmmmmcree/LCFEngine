#pragma once

#include "VulkanBuffer.h"

namespace lcf::render {
    class VulkanContext;

    class VulkanBufferProxy
    {
        using Self = VulkanBufferProxy;
    public:
        VulkanBufferProxy() = default;
        ~VulkanBufferProxy() = default;
        VulkanBufferProxy(const Self &) = default;
        Self & operator=(const Self &) = default;
        VulkanBufferProxy(Self &&) = default;
        Self & operator=(Self &&) = default;
    private:
        bool create(VulkanContext * context_p,
            const vk::BufferCreateInfo & buffer_info,
            const MemoryAllocationCreateInfo & memory_info);
        uint32_t getSize() const noexcept { return m_buffer_sp->getSize(); }
        vk::Buffer getHandle() const noexcept { return m_buffer_sp->getHandle(); }
        std::byte * getMappedMemoryPtr() const noexcept { return m_buffer_sp->getMappedMemoryPtr(); }
        const vk::DeviceAddress & getDeviceAddress() const noexcept { return m_device_address; }
        const VulkanBuffer::SharedPointer & getResource() const noexcept { return m_buffer_sp; }
    private:
        VulkanBuffer::SharedPointer m_buffer_sp;
        vk::DeviceAddress m_device_address;
    };
}