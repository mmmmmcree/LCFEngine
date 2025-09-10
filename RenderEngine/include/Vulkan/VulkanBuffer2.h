#include "common/GPUBuffer2.h"
#include "VulkanMemoryAllocator.h"
#include <vulkan/vulkan.hpp>
#include "VulkanTimelineSemaphore.h"
#include "PointerDefs.h"
#include <queue>
#include <span>

namespace lcf::render {
    class VulkanContext;

    struct BufferWriteSegment;

    class VulkanBuffer2 : public PointerDefs<VulkanBuffer2>
    {
        using Self = VulkanBuffer2;
    public:
        VulkanBuffer2() = default;
        ~VulkanBuffer2() = default;
        bool create(VulkanContext * context_p);
        bool isCreated() const { return m_buffer.get(); }
        void setSize(uint32_t size_in_bytes);
        void resize(uint32_t size_in_bytes);
        /*
            set data is lazy operation, the method only record data segment to be written,
            the actual writing operation is performed by endWrite() method.
            endWrite should organize all the data segments, if any data segment is out of buffer range, buffer should be resized.
            resize operation should be performed carefully, due to the buffer might be pending for reading
            resize
            {
                if (is pending)
            }
        */
        void addWriteSegment(const BufferWriteSegment &segment) noexcept;
        void commitWriteSegments();

        void copyFrom(const VulkanBuffer2 &src,
            uint32_t data_size_in_bytes = 0u,
            uint32_t src_offset_in_bytes = 0u,
            uint32_t dst_offset_in_bytes = 0u);
        uint32_t getSize() const { return m_size; }
        Self & setUsagePattern(GPUBufferPattern pattern) { m_pattern = pattern; return *this; }
        GPUBufferPattern getUsagePattern() const { return m_pattern; }
        Self & addUsageFlags(vk::BufferUsageFlags flags) { m_usage_flags |= flags; return *this; }
        vk::BufferUsageFlags getUsageFlags() const { return m_usage_flags; }
        Self & setUsage(GPUBufferUsage usage);
        Self & setSharingMode(vk::SharingMode sharing_mode) { m_sharing_mode = sharing_mode; return *this; }
        vk::Buffer getHandle() const { return m_buffer.get(); }
        vk::DeviceAddress getDeviceAddress() const;
        std::byte * getMappedMemoryPtr() const { return m_mapped_memory_p; }
    protected:
        void writeSegment(const BufferWriteSegment &segment);
        void writeSegmentByStaging(const BufferWriteSegment &segment);
    protected:
        VulkanContext * m_context_p = nullptr;
        VMAUniqueBuffer m_buffer;
        uint32_t m_size = 0;
        GPUBufferPattern m_pattern = GPUBufferPattern::eDynamic;
        std::byte *m_mapped_memory_p = nullptr;
        vk::SharingMode m_sharing_mode = vk::SharingMode::eExclusive;
        vk::BufferUsageFlags m_usage_flags;

        VulkanTimelineSemaphore::SharedPointer m_timeline_semaphore_sp;
        decltype(Self::writeSegment) m_write_segment_method = nullptr; // due to GPUBufferPattern
    };

    struct BufferWriteSegment
    {
        BufferWriteSegment(std::span<const std::byte> _data ,uint32_t _offset_in_bytes = 0u) :
            data(_data), offset_in_bytes(_offset_in_bytes) {}
        auto operator<=>(const BufferWriteSegment& other) const noexcept
        { return offset_in_bytes <=> other.offset_in_bytes; };
        std::span<const std::byte> data;
        uint32_t offset_in_bytes = 0;
    };
}