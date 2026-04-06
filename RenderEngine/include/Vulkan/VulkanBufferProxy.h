#pragma once

#include <vulkan/vulkan.hpp>
#include "vulkan_fwd_decls.h"
#include "common/render_enums.h"
#include "BufferWriteSegment.h"
#include "resource_utils.h"

namespace lcf::render {
    class VulkanBufferProxy
    {
        using Self = VulkanBufferProxy;
    public:
        VulkanBufferProxy() = default;
        ~VulkanBufferProxy();
        VulkanBufferProxy(const Self &) = default;
        Self & operator=(const Self &) = default;
        VulkanBufferProxy(Self &&) = default;
        Self & operator=(Self &&) = default;
    public:
        bool create(VulkanContext * context_p, uint64_t size_in_bytes);
        bool recreate(uint64_t size_in_bytes);
        bool isCreated() const noexcept { return m_buffer_rp and this->getHandle(); }
        void writeSegmentDirectly(const BufferWriteSegment &segment, uint64_t dst_offset_in_bytes = 0u) noexcept;
        void writeSegmentsDirectly(const BufferWriteSegments &segments, uint64_t dst_offset_in_bytes = 0u) noexcept;
        uint64_t getSizeInBytes() const noexcept;
        vk::Buffer getHandle() const noexcept;
        std::span<std::byte> getMappedMemorySpan() const noexcept;
        const vk::DeviceAddress & getDeviceAddress() const noexcept { return m_device_address; }
        const ResourcePointer<VulkanBuffer> & getResource() const noexcept { return m_buffer_rp; }
        Self & setUsage(GPUBufferUsage usage) noexcept { m_buffer_usage = usage; return *this; }
        Self & setPattern(GPUBufferPattern pattern) noexcept { m_buffer_pattern = pattern; return *this; }
        GPUBufferUsage getUsage() const noexcept { return m_buffer_usage; }
        GPUBufferPattern getPattern() const noexcept { return m_buffer_pattern; }
        vk::DescriptorBufferInfo generateBufferInfo(vk::DeviceSize offset, vk::DeviceSize range) const noexcept;
    private:
        VulkanContext * m_context_p = nullptr;
        ResourcePointer<VulkanBuffer> m_buffer_rp;
        vk::DeviceAddress m_device_address;
        GPUBufferUsage m_buffer_usage = GPUBufferUsage::eUndefined;
        GPUBufferPattern m_buffer_pattern = GPUBufferPattern::eDynamic;
    };
}