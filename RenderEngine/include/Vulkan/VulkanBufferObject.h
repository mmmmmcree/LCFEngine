#include "common/enum_types.h"
#include "common/GPUResource.h"
#include "VulkanMemoryAllocator.h"
#include "VulkanTimelineSemaphore.h"
#include "VulkanBufferResource.h"
#include "PointerDefs.h"
#include "as_bytes.h"
#include <span>
// #include <boost/icl/interval_map.hpp> //- use interval tree to manage write segments
#include <deque>
#include <vulkan/vulkan.hpp>

namespace lcf::render {
    class VulkanContext;

    class BufferWriteSegment
    {
    public:
        BufferWriteSegment() = default;
        BufferWriteSegment(ByteView bytes, size_t offset_in_bytes = 0u) noexcept :
            m_data(bytes), m_offset_in_bytes(offset_in_bytes) {}
        BufferWriteSegment(const BufferWriteSegment& other) = default;
        BufferWriteSegment(BufferWriteSegment&& other) noexcept = default;
        BufferWriteSegment& operator=(const BufferWriteSegment& other) = default;
        ByteView getDataView() const noexcept { return m_data; }
        const std::byte * getData() const noexcept { return m_data.data(); }
        size_t getSizeInBytes() const noexcept { return m_data.size_bytes(); }
        size_t getBeginOffsetInBytes() const noexcept { return m_offset_in_bytes; }
        size_t getEndOffsetInBytes() const noexcept { return m_offset_in_bytes + m_data.size_bytes(); }
        bool operator==(const BufferWriteSegment& other) const noexcept; // equality rule for boost::icl::interval_map
        BufferWriteSegment & operator+=(const BufferWriteSegment& other) noexcept; //merge rule for boost::icl::interval_map
    private:
        ByteView m_data;
        size_t m_offset_in_bytes = 0;
    };

    class VulkanBufferObject : public STDPointerDefs<VulkanBufferObject>
    {
        using Self = VulkanBufferObject;
    public:
        IMPORT_POINTER_DEFS(STDPointerDefs<VulkanBufferObject>);
        // using WriteSegments = boost::icl::interval_map<uint32_t, BufferWriteSegment>; //- use interval tree to manage write segments
        using WriteSegments = std::deque<BufferWriteSegment>;
        using ExecuteWriteSequenceMethod = void (Self::*)();
        VulkanBufferObject() = default;
        bool create(VulkanContext * context_p);
        bool isCreated() const noexcept { return m_buffer_up and m_buffer_up->getHandle(); }
        Self & setSize(uint32_t size_in_bytes) noexcept;
        Self & setUsage(GPUBufferUsage usage) noexcept;
        Self & setPattern(GPUBufferPattern pattern) noexcept;
        Self & addWriteSegment(const BufferWriteSegment &segment) noexcept; // overwrite if overlaps
        Self & addWriteSegmentIfAbsent(const BufferWriteSegment &segment) noexcept; // don't overwrite if overlaps
        void commitWriteSegments();
        uint32_t getSize() const noexcept { return m_size; }
        vk::Buffer getHandle() const noexcept { return m_buffer_up ? m_buffer_up->getHandle() : nullptr; }
        const vk::DeviceAddress & getDeviceAddress() const noexcept { return m_device_address; }
        std::byte * getMappedMemoryPtr() const noexcept { return m_buffer_up->getMappedMemoryPtr(); }
    private:
        void recreate(uint32_t size_in_bytes);
        Self & resize(uint32_t size_in_bytes);
        void copyFromBufferWithBarriers(vk::Buffer src,
            uint32_t data_size_in_bytes,
            uint32_t src_offset_in_bytes = 0u,
            uint32_t dst_offset_in_bytes = 0u);
        void writeSegmentsDirectly(const WriteSegments &segments, uint32_t dst_offset_in_bytes = 0u);
        void executeWriteSequence();
        void executeCpuWriteSequence();
        void executeGpuWriteSequence();
        void executeGpuWriteSequenceWithoutCmd();
        void updateWriteBounds(uint32_t lower_bound, uint32_t upper_bound);
        void resetWriteBounds();
    private:
        VulkanContext * m_context_p = nullptr;
        VulkanTimelineSemaphore::UniquePointer m_timeline_semaphore_up; // up to GPUBufferUsage
        VulkanBufferResource::UniquePointer m_buffer_up;
        ExecuteWriteSequenceMethod m_execute_write_sequence_method = nullptr; // up to GPUBufferPattern
        vk::DeviceAddress m_device_address = 0u;
        uint32_t m_size = 0;
        uint32_t m_write_segment_lower_bound = -1u;
        uint32_t m_write_segment_upper_bound = 0u;
        GPUBufferUsage m_usage = GPUBufferUsage::eUndefined;
        GPUBufferPattern m_pattern = GPUBufferPattern::eDynamic;
        vk::PipelineStageFlags2 m_stage_flags = vk::PipelineStageFlagBits2KHR::eAllGraphics;
        vk::AccessFlags2 m_access_flags = vk::AccessFlagBits2KHR::eMemoryRead;
        WriteSegments m_write_segments;
    };
}