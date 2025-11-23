#include "VulkanBufferObject.h"
#include "VulkanContext.h"
#include "log.h"
#include <boost/align.hpp>
#include "vulkan_utililtie.h"

using namespace lcf::render;

VulkanBufferObject::VulkanBufferObject(Self &&other) :
    m_context_p(std::exchange(other.m_context_p, nullptr)),
    m_timeline_semaphore_up(std::move(other.m_timeline_semaphore_up)),
    m_buffer_sp(std::move(m_buffer_sp)),
    m_execute_write_sequence_method(std::exchange(other.m_execute_write_sequence_method, nullptr)),
    m_device_address(std::exchange(other.m_device_address, 0u)),
    m_usage(std::exchange(other.m_usage, GPUBufferUsage::eUndefined)),
    m_pattern(other.m_pattern),
    m_stage_flags(other.m_stage_flags),
    m_access_flags(other.m_access_flags),
    m_write_segments(std::move(other.m_write_segments))
{
}

VulkanBufferObject & VulkanBufferObject::operator=(Self &&other)
{
    m_context_p = std::exchange(other.m_context_p, nullptr);
    m_timeline_semaphore_up = std::move(other.m_timeline_semaphore_up);
    m_buffer_sp = std::move(m_buffer_sp);
    m_execute_write_sequence_method = std::exchange(other.m_execute_write_sequence_method, nullptr);
    m_device_address = std::exchange(other.m_device_address, 0u);
    m_usage = std::exchange(other.m_usage, GPUBufferUsage::eUndefined);
    m_pattern = other.m_pattern;
    m_stage_flags = other.m_stage_flags;
    m_access_flags = other.m_access_flags;
    m_write_segments = std::move(other.m_write_segments);
    return *this;
}

bool VulkanBufferObject::create(VulkanContext *context_p)
{
    m_context_p = context_p;
    vk::BufferUsageFlags usage_flags;
    vk::MemoryPropertyFlags memory_flags;
    switch (m_usage) {
        case GPUBufferUsage::eUndefined : {
            std::runtime_error error("Undefined buffer usage");
            lcf_log_error(error.what());
            throw error;
        } break;
        case GPUBufferUsage::eVertex : {
            m_stage_flags = vk::PipelineStageFlagBits2::eVertexInput;
            m_access_flags = vk::AccessFlagBits2::eVertexAttributeRead;
        } break;
        case GPUBufferUsage::eIndex : {
            m_stage_flags = vk::PipelineStageFlagBits2::eVertexInput;
            m_access_flags = vk::AccessFlagBits2::eIndexRead;
        } break;
        case GPUBufferUsage::eUniform : {
            m_stage_flags = vk::PipelineStageFlagBits2::eVertexShader | vk::PipelineStageFlagBits2::eFragmentShader;
            m_access_flags = vk::AccessFlagBits2::eShaderRead;
        } break;
        case GPUBufferUsage::eShaderStorage : {
            m_stage_flags = vk::PipelineStageFlagBits2::eVertexShader;
            m_access_flags = vk::AccessFlagBits2::eShaderRead;
        } break;
        case GPUBufferUsage::eIndirect : {
            m_stage_flags = vk::PipelineStageFlagBits2::eDrawIndirect;
            m_access_flags = vk::AccessFlagBits2::eIndirectCommandRead;
        } break;
        case GPUBufferUsage::eStaging : {
            m_stage_flags = vk::PipelineStageFlagBits2KHR::eAllGraphics;
            m_access_flags = vk::AccessFlagBits2KHR::eMemoryRead;
        } break;
    }
    switch (m_pattern) {
        case GPUBufferPattern::eDynamic : {
            m_execute_write_sequence_method = &Self::executeWriteSequence;
        } break;
        case GPUBufferPattern::eStatic : {
            m_execute_write_sequence_method = &Self::executeGpuWriteSequence;
        } break;
    }
    if (m_usage != GPUBufferUsage::eStaging) {
        m_timeline_semaphore_up = VulkanTimelineSemaphore::makeUnique();
        m_timeline_semaphore_up->create(m_context_p);
    } else {
        m_execute_write_sequence_method = &Self::executeCpuWriteSequence;
    }
    this->recreate(m_size);
    return this->isCreated();
}

void lcf::render::VulkanBufferObject::recreate(uint32_t size_in_bytes)
{
    if (size_in_bytes == 0u or (this->isCreated() and m_size == size_in_bytes)) { return; }
    vk::BufferUsageFlags usage_flags;
    vk::MemoryPropertyFlags memory_flags;
    switch (m_usage) {
        case GPUBufferUsage::eVertex : {
            usage_flags = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst;
        } break;
        case GPUBufferUsage::eIndex : {
            usage_flags = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst;
        } break;
        case GPUBufferUsage::eUniform : {
            usage_flags = vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst;
        } break;
        case GPUBufferUsage::eShaderStorage : {
            usage_flags = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eShaderDeviceAddress;
        } break;
        case GPUBufferUsage::eIndirect : {
            usage_flags = vk::BufferUsageFlagBits::eStorageBuffer |
                vk::BufferUsageFlagBits::eTransferSrc |
                vk::BufferUsageFlagBits::eTransferDst |
                vk::BufferUsageFlagBits::eShaderDeviceAddress |
                vk::BufferUsageFlagBits::eIndirectBuffer;
        } break;
        case GPUBufferUsage::eStaging : {
            usage_flags = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst;
        } break;
    }
    switch (m_pattern) {
        case GPUBufferPattern::eDynamic : {
            memory_flags = vk::MemoryPropertyFlagBits::eHostVisible;
        } break;
        case GPUBufferPattern::eStatic : {
            memory_flags = vk::MemoryPropertyFlagBits::eDeviceLocal;
        } break;
    }
    m_size = boost::alignment::align_up(size_in_bytes, 4u);
    vk::BufferCreateInfo buffer_info = {{}, m_size, usage_flags, vk::SharingMode::eExclusive};
    auto & memory_allocator = m_context_p->getMemoryAllocator();
    m_buffer_sp = VulkanBufferResource::makeShared();
    m_buffer_sp->create(memory_allocator, buffer_info, {memory_flags});
    if (usage_flags & vk::BufferUsageFlagBits::eShaderDeviceAddress) {
        m_device_address = m_context_p->getDevice().getBufferAddress(m_buffer_sp->getHandle());
    }
}

VulkanBufferObject & VulkanBufferObject::setSize(uint32_t size_in_bytes) noexcept
{
    if (not this->isCreated()) { m_size = size_in_bytes; }
    return *this;
}

VulkanBufferObject & VulkanBufferObject::setUsage(GPUBufferUsage usage) noexcept
{
    if (not this->isCreated()) { m_usage = usage; }
    return *this;
}

VulkanBufferObject & VulkanBufferObject::setPattern(GPUBufferPattern pattern) noexcept
{
    if (not this->isCreated()) { m_pattern = pattern; }
    return *this;
}

VulkanBufferObject &VulkanBufferObject::resize(uint32_t size_in_bytes, VulkanCommandBufferObject & cmd)
{
    auto old_buffer_up = std::move(m_buffer_sp);
    uint32_t old_size = m_size;
    this->recreate(size_in_bytes);
    if (old_buffer_up) {
        this->addWriteSegmentIfAbsent({as_bytes_from_ptr(m_buffer_sp->getMappedMemoryPtr(), std::min(old_size, size_in_bytes))});
        cmd.acquireResource(std::move(old_buffer_up));
        this->commitWriteSegments(cmd);
    }
    return *this;
}

VulkanBufferObject & VulkanBufferObject::addWriteSegment(const BufferWriteSegment &segment) noexcept
{
    m_write_segments.add(segment);
    return *this;
}

VulkanBufferObject & VulkanBufferObject::addWriteSegmentIfAbsent(const BufferWriteSegment &segment) noexcept
{
    m_write_segments.addIfAbsent(segment);
    return *this;
}

void VulkanBufferObject::commitWriteSegments(VulkanCommandBufferObject & cmd)
{
    if (m_write_segments.empty()) { return; }
    (this->*m_execute_write_sequence_method)(cmd);
    cmd.acquireResource(m_buffer_sp);
    m_write_segments.clear();
}

void lcf::render::VulkanBufferObject::copyFromBufferWithBarriers(VulkanCommandBufferObject & cmd, vk::Buffer src, uint32_t data_size_in_bytes, uint32_t src_offset_in_bytes, uint32_t dst_offset_in_bytes)
{
    if (not m_timeline_semaphore_up->isTargetReached()) {
        vk::BufferMemoryBarrier2 pre_buffer_barrier;
        pre_buffer_barrier.setSrcStageMask(m_stage_flags)
            .setSrcAccessMask(m_access_flags)
            .setDstStageMask(vk::PipelineStageFlagBits2KHR::eAllTransfer)
            .setDstAccessMask(vk::AccessFlagBits2KHR::eTransferWrite)
            .setBuffer(this->getHandle())
            .setOffset(0)
            .setSize(VK_WHOLE_SIZE);
        vk::DependencyInfo pre_dependency;
        pre_dependency.setBufferMemoryBarriers(pre_buffer_barrier);
        cmd.pipelineBarrier2(pre_dependency);
    }
    vk::BufferMemoryBarrier2 post_barrier;
    post_barrier.setSrcStageMask(vk::PipelineStageFlagBits2KHR::eAllTransfer)
        .setSrcAccessMask(vk::AccessFlagBits2KHR::eTransferWrite)
        .setDstStageMask(m_stage_flags)
        .setDstAccessMask(m_access_flags)
        .setBuffer(this->getHandle())
        .setOffset(dst_offset_in_bytes)
        .setSize(data_size_in_bytes);
    vk::DependencyInfo post_dependency;
    post_dependency.setBufferMemoryBarriers(post_barrier);
    vk::BufferCopy copy_region(src_offset_in_bytes, dst_offset_in_bytes, data_size_in_bytes);
    cmd.copyBuffer(src, this->getHandle(), copy_region);
    cmd.pipelineBarrier2(post_dependency);
}

void VulkanBufferObject::writeSegmentsDirectly(const WriteSegments &segments, uint32_t dst_offset_in_bytes)
{
    for (const auto & write_segment : segments) {
        memcpy(m_buffer_sp->getMappedMemoryPtr() + write_segment.getBeginOffsetInBytes() + dst_offset_in_bytes,
            write_segment.getData(), write_segment.getSizeInBytes());
    }
    m_buffer_sp->flush(m_write_segments.getLowerBoundInBytes() + dst_offset_in_bytes, m_write_segments.getValidSizeInBytes());
}

void lcf::render::VulkanBufferObject::executeWriteSequence(VulkanCommandBufferObject & cmd)
{
    if (m_timeline_semaphore_up->isTargetReached()) {
        this->executeCpuWriteSequence(cmd);
    } else {
        this->executeGpuWriteSequence(cmd);
    }
    m_timeline_semaphore_up->increaseTargetValue();
    cmd.addSignalSubmitInfo(m_timeline_semaphore_up->generateSubmitInfo());
}

void lcf::render::VulkanBufferObject::executeCpuWriteSequence(VulkanCommandBufferObject & cmd)
{
    uint32_t write_required_size = m_write_segments.getUpperBoundInBytes();
    if (write_required_size > m_size) [[unlikely]] {
        if (m_buffer_sp) {
            this->addWriteSegmentIfAbsent({as_bytes_from_ptr(m_buffer_sp->getMappedMemoryPtr(), m_size)});
            cmd.acquireResource(std::move(m_buffer_sp));
        }
        this->recreate(write_required_size); //todo use custom enlarge strategy
    }
    this->writeSegmentsDirectly(m_write_segments);
}

void lcf::render::VulkanBufferObject::executeGpuWriteSequence(VulkanCommandBufferObject & cmd)
{
    uint32_t write_required_size = m_write_segments.getUpperBoundInBytes();
    if (write_required_size > m_size) [[unlikely]] {
        auto old_buffer_sp = std::move(m_buffer_sp);
        uint32_t old_size = m_size;
        this->recreate(write_required_size); //todo use custom enlarge strategy
        if (old_buffer_sp) {
            this->copyFromBufferWithBarriers(cmd, old_buffer_sp->getHandle(), old_size);
            cmd.acquireResource(old_buffer_sp);
        }
    }
    uint32_t dst_offset = -m_write_segments.getLowerBoundInBytes();
    auto staging_buffer = VulkanBufferObject::makeUnique();
    staging_buffer->setUsage(GPUBufferUsage::eStaging)
        .setSize(write_required_size + dst_offset)
        .create(m_context_p);
    staging_buffer->writeSegmentsDirectly(m_write_segments, dst_offset);
    cmd.acquireResource(staging_buffer->m_buffer_sp);
    this->copyFromBufferWithBarriers(cmd, staging_buffer->getHandle(), staging_buffer->getSize(), 0, dst_offset);
}