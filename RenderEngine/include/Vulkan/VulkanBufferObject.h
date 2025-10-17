#pragma once

#include "common/enum_types.h"
#include "common/GPUResource.h"
#include "VulkanMemoryAllocator.h"
#include "VulkanTimelineSemaphore.h"
#include "VulkanBufferResource.h"
#include "VulkanCommandBufferObject.h"
#include "PointerDefs.h"
#include "BufferWriteSegment.h"
#include <span>
#include <deque>
#include <vulkan/vulkan.hpp>

namespace lcf::render {
    class VulkanContext;

    class VulkanBufferObject : public STDPointerDefs<VulkanBufferObject>
    {
        using Self = VulkanBufferObject;
    public:
        IMPORT_POINTER_DEFS(STDPointerDefs<VulkanBufferObject>);
        using WriteSegments = BufferWriteSegments;
        using ExecuteWriteSequenceMethod = void (Self::*)(VulkanCommandBufferObject &);
        VulkanBufferObject() = default;
        VulkanBufferObject(const Self &) = delete;
        VulkanBufferObject(Self &&) = delete;
        Self & operator=(const Self &) = delete;
        Self & operator=(Self &&) = delete;
        bool create(VulkanContext * context_p);
        bool isCreated() const noexcept { return m_buffer_sp and m_buffer_sp->getHandle(); }
        Self & setSize(uint32_t size_in_bytes) noexcept;
        Self & setUsage(GPUBufferUsage usage) noexcept;
        Self & setPattern(GPUBufferPattern pattern) noexcept;
        Self & addWriteSegment(const BufferWriteSegment &segment) noexcept; // overwrite if overlaps
        Self & addWriteSegmentIfAbsent(const BufferWriteSegment &segment) noexcept; // don't overwrite if overlaps
        void commitWriteSegments(VulkanCommandBufferObject & cmd);
        uint32_t getSize() const noexcept { return m_size; }
        vk::Buffer getHandle() const noexcept { return m_buffer_sp ? m_buffer_sp->getHandle() : nullptr; }
        const vk::DeviceAddress & getDeviceAddress() const noexcept { return m_device_address; }
        std::byte * getMappedMemoryPtr() const noexcept { return m_buffer_sp->getMappedMemoryPtr(); }
    private:
        void recreate(uint32_t size_in_bytes);
        Self & resize(uint32_t size_in_bytes, VulkanCommandBufferObject & cmd);
        void copyFromBufferWithBarriers(VulkanCommandBufferObject & cmd,
            vk::Buffer src,
            uint32_t data_size_in_bytes,
            uint32_t src_offset_in_bytes = 0u,
            uint32_t dst_offset_in_bytes = 0u);
        void writeSegmentsDirectly(const WriteSegments &segments, uint32_t dst_offset_in_bytes = 0u);
        void executeWriteSequence(VulkanCommandBufferObject & cmd);
        void executeCpuWriteSequence(VulkanCommandBufferObject & cmd);
        void executeGpuWriteSequence(VulkanCommandBufferObject & cmd);
    private:
        VulkanContext * m_context_p = nullptr;
        VulkanTimelineSemaphore::UniquePointer m_timeline_semaphore_up; // up to GPUBufferUsage
        VulkanBufferResource::SharedPointer m_buffer_sp;
        ExecuteWriteSequenceMethod m_execute_write_sequence_method = nullptr; // up to GPUBufferPattern
        vk::DeviceAddress m_device_address = 0u;
        uint32_t m_size = 0;
        GPUBufferUsage m_usage = GPUBufferUsage::eUndefined;
        GPUBufferPattern m_pattern = GPUBufferPattern::eDynamic;
        vk::PipelineStageFlags2 m_stage_flags = vk::PipelineStageFlagBits2KHR::eAllGraphics;
        vk::AccessFlags2 m_access_flags = vk::AccessFlagBits2KHR::eMemoryRead;
        WriteSegments m_write_segments;
    };
}