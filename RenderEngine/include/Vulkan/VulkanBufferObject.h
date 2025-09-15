#include "common/GPUBuffer2.h"
#include "common/GPUResource.h"
#include "VulkanMemoryAllocator.h"
#include "VulkanTimelineSemaphore.h"
#include "PointerDefs.h"
#include <span>
#include <boost/icl/interval_map.hpp>
#include <vulkan/vulkan.hpp>

namespace lcf::render {
    class VulkanContext;

    class BufferWriteSegment
    {
    public:
        BufferWriteSegment() = default;
        template <std::ranges::range Range>
        BufferWriteSegment(const Range& data, uint32_t offset_in_bytes = 0u) :
            m_data(std::as_bytes(std::span(data))), m_offset_in_bytes(offset_in_bytes) {}
        BufferWriteSegment(std::span<const std::byte> data, uint32_t offset_in_bytes = 0u) :
            m_data(data), m_offset_in_bytes(offset_in_bytes) {}
        BufferWriteSegment(const BufferWriteSegment& other) = default;
        BufferWriteSegment(BufferWriteSegment&& other) noexcept = default;
        BufferWriteSegment& operator=(const BufferWriteSegment& other) = default;
        std::span<const std::byte> getDataSpan() const noexcept { return m_data; }
        const std::byte * getData() const noexcept { return m_data.data(); }
        uint32_t getBeginOffsetInBytes() const noexcept { return m_offset_in_bytes; }
        uint32_t getEndOffsetInBytes() const noexcept { return m_offset_in_bytes + m_data.size_bytes(); }
        bool operator==(const BufferWriteSegment& other) const noexcept; // equality rule for boost::icl::interval_map
        BufferWriteSegment & operator+=(const BufferWriteSegment& other) noexcept; //merge rule for boost::icl::interval_map
    private:
        std::span<const std::byte> m_data;
        uint32_t m_offset_in_bytes = 0;
    };

    class VulkanBufferResource : public GPUResource, public PointerDefs<VulkanBufferResource>
    {
        using Self = VulkanBufferResource;
    public:
        IMPORT_POINTER_DEFS(VulkanBufferResource);
        VulkanBufferResource() = default;
        bool create(VulkanContext * context_p, const vk::BufferCreateInfo &buffer_info, vk::MemoryPropertyFlags memory_flags);
        vk::Buffer getHandle() const noexcept { return m_buffer.get(); }
        std::byte * getMappedMemoryPtr() const noexcept { return m_mapped_memory_p; }
    private:
        VMAUniqueBuffer m_buffer;
        std::byte *m_mapped_memory_p = nullptr;
    };
    
    class VulkanBufferObject : public PointerDefs<VulkanBufferObject>
    {
        using Self = VulkanBufferObject;
    public:
        IMPORT_POINTER_DEFS(VulkanBufferObject);
        using WriteSegmentIntervalMap = boost::icl::interval_map<uint32_t, BufferWriteSegment>;
        using ExecuteWriteSequenceMethod = void (Self::*)();
        VulkanBufferObject() = default;
        bool create(VulkanContext * context_p);
        bool isCreated() const noexcept { return m_buffer_sp->getHandle(); }
        Self & setSize(uint32_t size_in_bytes);
        void addWriteSegment(const BufferWriteSegment &segment) noexcept; // overwrite if overlaps
        void addWriteSegmentIfAbsent(const BufferWriteSegment &segment) noexcept; // don't overwrite if overlaps
        void commitWriteSegments();
        uint32_t getSize() const noexcept { return m_size; }
        vk::BufferUsageFlags getUsageFlags() const noexcept { return m_usage_flags; }
        Self & setUsage(GPUBufferUsage usage) noexcept;
        vk::Buffer getHandle() const noexcept { return m_buffer_sp->getHandle(); }
        vk::DeviceAddress getDeviceAddress() const;
        std::byte * getMappedMemoryPtr() const noexcept { return m_buffer_sp->getMappedMemoryPtr(); }
    private:
        Self & resize(uint32_t size_in_bytes);
        Self & setUsagePattern(GPUBufferPattern pattern) noexcept { m_pattern = pattern; return *this; }
        Self & addUsageFlags(vk::BufferUsageFlags flags) noexcept { m_usage_flags |= flags; return *this; }
        const VulkanBufferResource::SharedPointer & getBufferResource() const noexcept { return m_buffer_sp; }
        void copyFromBufferWithBarriers(vk::Buffer src,
            uint32_t data_size_in_bytes,
            uint32_t src_offset_in_bytes = 0u,
            uint32_t dst_offset_in_bytes = 0u);
        void executeWriteSequence();
        void executeCpuWriteSequence();
        void executeGpuWriteSequence();
        void executeGpuWriteSequenceImediate();
    private:
        VulkanContext * m_context_p = nullptr;
        VulkanTimelineSemaphore::SharedPointer m_timeline_semaphore_sp;
        VulkanBufferResource::SharedPointer m_buffer_sp;
        uint32_t m_size = 0;
        GPUBufferPattern m_pattern = GPUBufferPattern::eDynamic;
        vk::SharingMode m_sharing_mode = vk::SharingMode::eExclusive;
        vk::PipelineStageFlags2 m_stage_flags = vk::PipelineStageFlagBits2KHR::eAllGraphics;
        vk::AccessFlags2 m_access_flags = vk::AccessFlagBits2KHR::eMemoryRead;
        vk::BufferUsageFlags m_usage_flags;
        WriteSegmentIntervalMap m_write_segment_map;
        ExecuteWriteSequenceMethod m_execute_write_sequence_method = nullptr; // up to GPUBufferPattern
        bool m_is_prev_write_by_staging = false;
    };
}