#include "common/GPUBuffer2.h"
#include "VulkanResource.h"
#include "VulkanMemoryAllocator.h"
#include <vulkan/vulkan.hpp>
#include "VulkanTimelineSemaphore.h"
#include "PointerDefs.h"
#include <queue>
#include <span>

namespace lcf::render {
    class VulkanContext;

    struct BufferWriteSegment;

    class VulkanBuffer2 : public VulkanResource, public PointerDefs<VulkanBuffer2>
    {
        using Self = VulkanBuffer2;
    public:
        IMPORT_POINTER_DEFS(VulkanBuffer2);
        using WriteSegmentList = std::vector<BufferWriteSegment>;
        using WriteSegmentMethod = void (Self::*)(const WriteSegmentList &segment);
        VulkanBuffer2() = default;
        virtual ~VulkanBuffer2() override = default;
        bool create(VulkanContext * context_p);
        bool isCreated() const noexcept { return m_buffer.get(); }
        Self & setSize(uint32_t size_in_bytes);
        Self & resize(uint32_t size_in_bytes);
        void addWriteSegment(const BufferWriteSegment &segment) noexcept;
        void commitWriteSegments();
        uint32_t getSize() const noexcept { return m_size; }
        vk::BufferUsageFlags getUsageFlags() const noexcept { return m_usage_flags; }
        Self & setUsage(GPUBufferUsage usage) noexcept;
        vk::Buffer getHandle() const { return m_buffer.get(); }
        vk::DeviceAddress getDeviceAddress() const;
        std::byte * getMappedMemoryPtr() const { return m_mapped_memory_p; }
    private:
        Self & setUsagePattern(GPUBufferPattern pattern) noexcept { m_pattern = pattern; return *this; }
        Self & addUsageFlags(vk::BufferUsageFlags flags) noexcept { m_usage_flags |= flags; return *this; }
        void writeSegments(const WriteSegmentList &segments);
        void writeSegmentsByStaging(const WriteSegmentList &segments);
        void writeSegmentsByStagingImediate(const WriteSegmentList &segments);
    private:
        VulkanContext * m_context_p = nullptr;
        VMAUniqueBuffer m_buffer;
        uint32_t m_size = 0;
        GPUBufferPattern m_pattern = GPUBufferPattern::eDynamic;
        vk::SharingMode m_sharing_mode = vk::SharingMode::eExclusive;
        vk::PipelineStageFlags2 m_stage_flags = vk::PipelineStageFlagBits2KHR::eAllGraphics;
        vk::AccessFlags2 m_access_flags = vk::AccessFlagBits2KHR::eMemoryRead;
        vk::BufferUsageFlags m_usage_flags;
        std::byte *m_mapped_memory_p = nullptr;
        std::priority_queue<BufferWriteSegment> m_write_segments;
        WriteSegmentMethod m_write_segment_method = nullptr; // up to GPUBufferPattern
    };

    struct BufferWriteSegment
    {
        BufferWriteSegment() = default;
        template <std::ranges::range Range>
        BufferWriteSegment(const Range& _data, uint32_t _offset_in_bytes = 0u) :
            data(std::as_bytes(std::span(_data))), offset_in_bytes(_offset_in_bytes) {}
        BufferWriteSegment(std::span<const std::byte> _data, uint32_t _offset_in_bytes = 0u) :
            data(_data), offset_in_bytes(_offset_in_bytes) {}
        BufferWriteSegment(const BufferWriteSegment& other) = default;
        BufferWriteSegment(BufferWriteSegment&& other) noexcept = default;
        BufferWriteSegment& operator=(const BufferWriteSegment& other) = default;
        auto operator<=>(const BufferWriteSegment& other) const noexcept
        { return offset_in_bytes <=> other.offset_in_bytes; };
        std::span<const std::byte> data;
        uint32_t offset_in_bytes = 0;
    };
}