#pragma once

#include "common/GPUBuffer.h"
#include "VulkanMemoryAllocator.h"
#include <vulkan/vulkan.hpp>
#include "VulkanTimelineSemaphore.h"
#include "PointerDefs.h"
#include <queue>

namespace lcf::render {
    class VulkanContext;

    class VulkanBuffer : public GPUBuffer, public PointerDefs<VulkanBuffer>
    {
    public:
        VulkanBuffer(VulkanContext * context);
        ~VulkanBuffer() = default;
        bool create();
        void setSize(uint32_t size_in_bytes);
        void resize(uint32_t size_in_bytes);
        void setData(const void *data, uint32_t size_in_bytes);
        void setSubData(const void *data, uint32_t size_in_bytes, uint32_t offset_in_bytes);
        bool isCreated() const { return m_buffer.get(); }
        void copyFrom(const VulkanBuffer &src, uint32_t data_size_in_bytes = 0u, uint32_t src_offset_in_bytes = 0u, uint32_t dst_offset_in_bytes = 0u);
        void setUsageFlags(vk::BufferUsageFlags flags);
        vk::BufferUsageFlags getUsageFlags() const { return m_usage_flags; }
        bool hasUsageFlagsBits(vk::BufferUsageFlagBits usage_flag) const { return static_cast<bool>(m_usage_flags & usage_flag); }
        void setSharingMode(vk::SharingMode sharing_mode) { m_sharing_mode = sharing_mode; }
        vk::Buffer getHandle() const { return m_buffer.get(); }
        vk::DeviceAddress getDeviceAddress() const;
        std::byte * getMappedMemoryPtr() const { return m_mapped_memory_ptr; }
    protected:
        void setSubDataByMap(const void *data, uint32_t size_in_bytes, uint32_t offset_in_bytes);
        void setSubDataByStagingBuffer(const void *data, uint32_t size_in_bytes, uint32_t offset_in_bytes);
    protected:
        VulkanContext * m_context_p = nullptr;
        VMAUniqueBuffer m_buffer;
        std::byte *m_mapped_memory_ptr = nullptr;
        vk::BufferUsageFlags m_usage_flags;
        vk::SharingMode m_sharing_mode = vk::SharingMode::eExclusive;
    };

    class VulkanDynamicMultiBuffer : public PointerDefs<VulkanDynamicMultiBuffer>
    {
    public:
        using BufferList = std::vector<VulkanBuffer::UniquePointer>;
        VulkanDynamicMultiBuffer(VulkanContext * context);
        void setBufferCount(uint32_t count);
        void setUsageFlags(vk::BufferUsageFlags flags);
        void setUsagePattern(GPUBuffer::UsagePattern usage_pattern);
        void setSharingMode(vk::SharingMode sharing_mode);
        void setData(const void *data, uint32_t size_in_bytes);
        void setSubData(const void *data, uint32_t size_in_bytes, uint32_t offset_in_bytes);
        uint32_t getSingleBufferSize() const { return m_buffers[m_current_index]->getSize(); }
        void acquireNextBuffer() { m_current_index = (m_current_index + 1) % m_buffers.size(); }
        vk::Buffer getHandle() const { return m_buffers[m_current_index]->getHandle(); }
    private:
        VulkanContext * m_context_p = nullptr;
        bool m_is_created = false;
        BufferList m_buffers;
        uint32_t m_current_index = 0;
    };

    class VulkanStaticMultiBuffer : public VulkanBuffer, PointerDefs<VulkanStaticMultiBuffer>
    {
    public:
        IMPORT_POINTER_DEFS(VulkanStaticMultiBuffer);
        VulkanStaticMultiBuffer(VulkanContext * context);
        bool create();
        void setBufferCount(uint32_t count) { m_buffer_count = std::max(1u, count); }
        void setSingleBufferSize(uint32_t size_in_bytes);
        void acquireNextBuffer() { m_current_index = (m_current_index + 1) % m_buffer_count; }
        void setData(const void *data, uint32_t size_in_bytes);
        void setSubData(const void *data, uint32_t size_in_bytes, uint32_t offset_in_bytes);
        const uint32_t & getSingleBufferSize() const { return m_single_buffer_size; }
        uint32_t getDynamicOffset() const { return m_current_index * m_single_buffer_size; }
    private:
        using VulkanBuffer::setSize;
        using VulkanBuffer::resize;
    private:
        uint32_t m_buffer_count = 1;
        uint32_t m_current_index = 0;
        uint32_t m_single_buffer_size = 0;
    };

    class VulkanTimelineBuffer : public VulkanBuffer, PointerDefs<VulkanTimelineBuffer>
    {
    public:
        IMPORT_POINTER_DEFS(VulkanTimelineBuffer);
        using StagingBufferQueue = std::queue<VulkanTimelineBuffer::UniquePointer>;
        VulkanTimelineBuffer(VulkanContext * context);
        void setData(const void *data, uint32_t size_in_bytes);
        void setSubData(const void *data, uint32_t size_in_bytes, uint32_t offset_in_bytes);
        vk::SemaphoreSubmitInfo generateSubmitInfo() const;
        void beginWrite();
        void endWrite();
    private:
        using VulkanBuffer::setUsagePattern;
        bool isPendingComplete() const { return m_timeline_semaphore.isTargetReached(); }
        bool isUsingStagingBufferForWrite() const { return m_current_writing_buffer != this; }
    private:
        VulkanTimelineSemaphore m_timeline_semaphore;
        StagingBufferQueue m_staging_buffers;
        VulkanTimelineBuffer * m_current_writing_buffer = nullptr;
        bool m_is_writing = false;
    };
}