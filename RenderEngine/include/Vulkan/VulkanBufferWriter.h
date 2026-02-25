#pragma once

#include <vulkan/vulkan.hpp>
#include "vulkan_fwd_decls.h"
#include "common/render_enums.h"
#include "BufferWriteSegment.h"
#include <vector>

namespace lcf::render {
    class VulkanBufferWriter
    {
        friend class VulkanBufferObject;
        using Self = VulkanBufferWriter;
        using WriteBufferRequest = std::pair<VulkanBufferProxy *, const BufferWriteSegments *>;
        using WriteBufferRequestList = std::vector<WriteBufferRequest>;
    public:
        VulkanBufferWriter() = default;
        ~VulkanBufferWriter() noexcept;
        VulkanBufferWriter(const Self &) = delete;
        VulkanBufferWriter & operator=(const Self &) = delete;
        VulkanBufferWriter(Self &&) = default;
        Self & operator=(Self &&) = default;
    public:
        bool create(VulkanContext * context_p);
        Self & setPattern(GPUBufferPattern pattern) noexcept { m_pattern = pattern; return *this; }
        GPUBufferPattern getPattern() const noexcept { return m_pattern; }
        bool hasPendingOperations() const noexcept;
        Self & addWriteRequest(VulkanBufferProxy & buffer_proxy, const BufferWriteSegments & segments) noexcept;
        void write(VulkanCommandBufferObject & cmd) const noexcept;
    private:
        void executeCpuWriteSequence(
            VulkanCommandBufferObject & cmd,
            VulkanBufferProxy & buffer_proxy,
            const BufferWriteSegments &segments) const noexcept;
        void executeGpuWriteSequence(
            VulkanCommandBufferObject & cmd,
            VulkanBufferProxy & buffer_proxy,
            const BufferWriteSegments &segments) const noexcept;
        void copyFromBufferWithBarriers(
            VulkanCommandBufferObject & cmd,
            vk::Buffer src,
            VulkanBufferProxy & dst,
            uint64_t data_size_in_bytes,
            uint64_t src_offset_in_bytes = 0u,
            uint64_t dst_offset_in_bytes = 0u) const noexcept;
    private:
        VulkanContext * m_context_p = nullptr;
        mutable VulkanTimelineSemaphoreUniquePointer m_timeline_semaphore_up;
        GPUBufferPattern m_pattern = GPUBufferPattern::eDynamic;
        mutable WriteBufferRequestList m_write_requests;
    };
}