#pragma once

#include "common/render_enums.h"
#include "VulkanBuffer.h"
#include "BufferWriteSegment.h"

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
    public:
        bool create(VulkanContext * context_p, uint64_t size_in_bytes);
        bool recreate(uint64_t size_in_bytes);
        bool isCreated() const noexcept { m_buffer_sp and this->getHandle(); }
        void writeSegmentsDirectly(const BufferWriteSegments &segments, uint64_t dst_offset_in_bytes = 0u) noexcept;
        uint64_t getSizeInBytes() const noexcept { return m_buffer_sp->getSizeInBytes(); }
        vk::Buffer getHandle() const noexcept { return m_buffer_sp->getHandle(); }
        std::byte * getMappedMemoryPtr() const noexcept { return m_buffer_sp->getMappedMemoryPtr(); }
        const vk::DeviceAddress & getDeviceAddress() const noexcept { return m_device_address; }
        const VulkanBuffer::SharedPointer & getResource() const noexcept { return m_buffer_sp; }
        Self & setUsage(GPUBufferUsage usage) noexcept { m_buffer_usage = usage; return *this; }
        Self & setPattern(GPUBufferPattern pattern) noexcept { m_buffer_pattern = pattern; return *this; }
        GPUBufferUsage getUsage() const noexcept { return m_buffer_usage; }
        GPUBufferPattern getPattern() const noexcept { return m_buffer_pattern; }
        vk::DescriptorBufferInfo generateBufferInfo() const noexcept;
    private:
        VulkanContext * m_context_p = nullptr;
        VulkanBuffer::SharedPointer m_buffer_sp;
        vk::DeviceAddress m_device_address;
        GPUBufferUsage m_buffer_usage = GPUBufferUsage::eUndefined;
        GPUBufferPattern m_buffer_pattern = GPUBufferPattern::eDynamic;
    };
}