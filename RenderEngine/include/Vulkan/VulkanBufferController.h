#pragma once

#include "common/render_enums.h"
#include "VulkanTimelineSemaphore.h"
#include "VulkanBufferProxy.h"
#include "VulkanCommandBufferObject.h"
#include "BufferWriteSegment.h"

namespace lcf::render {
    class VulkanBufferController
    {
        using Self = VulkanBufferController;
    public:
        VulkanBufferController() = default;
        ~VulkanBufferController() = default;
        VulkanBufferController(const Self &) = delete;
        VulkanBufferController & operator=(const Self &) = delete;
        VulkanBufferController(Self &&) = default;
        Self & operator=(Self &&) = default;
    public:
        void create(VulkanContext * context_p);
        Self & setPattern(GPUBufferPattern pattern) noexcept { m_pattern = pattern; return *this; }
        GPUBufferPattern getPattern() const noexcept { return m_pattern; }
        void recreateBuffer(VulkanBufferProxy & buffer_proxy, uint32_t size_in_bytes);
        Self & resizeBuffer(VulkanCommandBufferObject & cmd, VulkanBufferProxy & buffer_proxy, uint32_t size_in_bytes);
        void executeWriteSequence(
            VulkanCommandBufferObject & cmd,
            std::span<VulkanBufferProxy> buffer_proxies,
            std::span<const BufferWriteSegments> segments_span);
    private:
        void executeCpuWriteSequence(
            VulkanCommandBufferObject & cmd,
            VulkanBufferProxy & buffer_proxy,
            const BufferWriteSegments &segments);
        void executeGpuWriteSequence(
            VulkanCommandBufferObject & cmd,
            VulkanBufferProxy & buffer_proxy,
            const BufferWriteSegments &segments);
        void writeSegmentsDirectly(
            VulkanBuffer & buffer,
            const BufferWriteSegments &segments,
            uint32_t dst_offset_in_bytes = 0u);
        void copyFromBufferWithBarriers(
            VulkanCommandBufferObject & cmd,
            vk::Buffer src,
            vk::Buffer dst,
            uint32_t data_size_in_bytes,
            uint32_t src_offset_in_bytes = 0u,
            uint32_t dst_offset_in_bytes = 0u);
    private:
        VulkanContext * m_context_p = nullptr;
        VulkanTimelineSemaphore::UniquePointer m_timeline_semaphore_up;
        GPUBufferPattern m_pattern = GPUBufferPattern::eDynamic;
    };
}