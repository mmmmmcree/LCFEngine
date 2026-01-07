#pragma once

#include "VulkanBufferWriter.h"
#include "VulkanBufferProxy.h"

namespace lcf::render {
    class VulkanBufferObjectGroup;

    class VulkanBufferObject2
    {
        friend class VulkanBufferObjectGroup;
        using Self = VulkanBufferObject2;
    public:
        VulkanBufferObject2() = default;
        ~VulkanBufferObject2() = default;
        VulkanBufferObject2(const Self &) = delete;
        VulkanBufferObject2(Self && other) = default;
        Self & operator=(const Self &) = delete;
        Self & operator=(Self && other) = default;
        bool create(VulkanContext * context_p, size_t size_in_bytes);
        bool isCreated() const noexcept { return m_buffer_proxy.isCreated(); }
        Self & setUsage(GPUBufferUsage usage) noexcept { m_buffer_proxy.setUsage(usage); return *this; }
        Self & setPattern(GPUBufferPattern pattern) noexcept;
        Self & resize(uint64_t size_in_bytes);
        Self & addWriteSegment(const BufferWriteSegment &segment) noexcept; // overwrite if overlaps
        Self & addWriteSegmentIfAbsent(const BufferWriteSegment &segment) noexcept; // don't overwrite if overlaps
        void commit(VulkanCommandBufferObject & cmd) noexcept;
        const vk::DeviceAddress & getDeviceAddress() const noexcept { return m_buffer_proxy.getDeviceAddress(); }
        uint64_t getSizeInBytes() const noexcept { return m_buffer_proxy.getSizeInBytes(); }
        vk::Buffer getHandle() const noexcept { return m_buffer_proxy.getHandle();  }
        vk::DescriptorBufferInfo generateBufferInfo() const noexcept { return m_buffer_proxy.generateBufferInfo(); }
    private:
        void prepareResize(VulkanCommandBufferObject & cmd, VulkanBufferWriter & writer) noexcept;
    private:
        VulkanBufferWriter m_writer;
        VulkanBufferProxy m_buffer_proxy;
        BufferWriteSegments m_write_segments;
        uint64_t m_required_size = 0;
    };
}