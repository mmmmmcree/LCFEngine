#include "VulkanBufferObject.h"
#include "VulkanContext.h"
#include <boost/align.hpp>
#include "vulkan_utililtie.h"

using namespace lcf::render;

bool VulkanBufferObject::create(VulkanContext * context_p)
{
    /*
        todo optimize create method,
        use different create stategy, like recreate, create without timeline,
    */
    m_context_p = context_p;
    if (not m_timeline_semaphore_sp) {
        m_timeline_semaphore_sp = VulkanTimelineSemaphore::makeShared();
        m_timeline_semaphore_sp->create(m_context_p);
    }
    auto memory_allocator = m_context_p->getMemoryAllocator();
    uint32_t alignment = 4;
    vk::PhysicalDeviceLimits limits = m_context_p->getPhysicalDevice().getProperties().limits;
    if (m_usage_flags & vk::BufferUsageFlagBits::eUniformBuffer) {
        alignment = limits.minUniformBufferOffsetAlignment;
    } else if (m_usage_flags & vk::BufferUsageFlagBits::eStorageBuffer) {
        alignment = limits.minStorageBufferOffsetAlignment;
    } else if (m_usage_flags & (vk::BufferUsageFlagBits::eStorageTexelBuffer|vk::BufferUsageFlagBits::eUniformTexelBuffer)) {
        alignment = limits.minTexelBufferOffsetAlignment;
    }
    m_size = boost::alignment::align_up(m_size, alignment);
    vk::BufferCreateInfo buffer_info = {{}, m_size, m_usage_flags, m_sharing_mode};
    vk::MemoryPropertyFlags memory_flags;
    switch (m_pattern) {
        case GPUBufferPattern::eDynamic : {
            memory_flags = vk::MemoryPropertyFlagBits::eHostVisible;
            m_execute_write_sequence_method = &Self::executeWriteSequence;
        } break;
        case GPUBufferPattern::eStatic : {
            memory_flags = vk::MemoryPropertyFlagBits::eDeviceLocal;
            m_execute_write_sequence_method = &Self::executeGpuWriteSequenceImediate;
        } break;
    }
    m_buffer_sp = VulkanBufferResource::makeShared();
    if (m_size == 0u) { return false; }
    m_buffer_sp->create(m_context_p, buffer_info, memory_flags);
    return this->isCreated();
}

VulkanBufferObject & VulkanBufferObject::setSize(uint32_t size_in_bytes)
{
    m_size = size_in_bytes;
    return *this;
}

VulkanBufferObject & VulkanBufferObject::resize(uint32_t size_in_bytes)
{
    auto old_buffer_sp = m_buffer_sp;
    uint32_t old_size = m_size;
    m_size = size_in_bytes;
    this->create(m_context_p);
    if (m_timeline_semaphore_sp->isTargetReached()) {
        memcpy(this->getMappedMemoryPtr(), old_buffer_sp->getMappedMemoryPtr(), std::min(old_size, m_size));
    } else {
        this->copyFromBufferWithBarriers(old_buffer_sp->getHandle(), std::min(old_size, m_size));
        auto cmd = m_context_p->getCurrentCommandBuffer();
        cmd->acquireResource(old_buffer_sp);
        m_timeline_semaphore_sp->increaseTargetValue();
        cmd->addSignalSubmitInfo(m_timeline_semaphore_sp->generateSubmitInfo());
    }
    return *this;
}

void VulkanBufferObject::addWriteSegment(const BufferWriteSegment &segment) noexcept
{
    auto segment_interval = boost::icl::interval<uint32_t>::right_open(segment.getBeginOffsetInBytes(), segment.getEndOffsetInBytes());
    m_write_segment_map.set(std::make_pair(segment_interval, segment));
}

void lcf::render::VulkanBufferObject::addWriteSegmentIfAbsent(const BufferWriteSegment &segment) noexcept
{
    auto segment_interval = boost::icl::interval<uint32_t>::right_open(segment.getBeginOffsetInBytes(), segment.getEndOffsetInBytes());
    m_write_segment_map.add(std::make_pair(segment_interval, segment));
}

void VulkanBufferObject::commitWriteSegments()
{
    if (m_write_segment_map.empty()) { return; }
    (this->*m_execute_write_sequence_method)();
}

VulkanBufferObject & VulkanBufferObject::setUsage(GPUBufferUsage usage) noexcept
{
    switch (usage) {
        case GPUBufferUsage::eVertex : {
            this->setUsagePattern(GPUBufferPattern::eStatic)
                .addUsageFlags(vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst);
            m_stage_flags = vk::PipelineStageFlagBits2::eVertexInput;
            m_access_flags = vk::AccessFlagBits2::eVertexAttributeRead;
        } break;
        case GPUBufferUsage::eIndex : {
        } break;
        case GPUBufferUsage::eUniform : {
            this->setUsagePattern(GPUBufferPattern::eDynamic)
                .addUsageFlags(vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst);
            m_stage_flags = vk::PipelineStageFlagBits2::eVertexShader | vk::PipelineStageFlagBits2::eFragmentShader;
            m_access_flags = vk::AccessFlagBits2::eShaderRead;
        } break;
        case GPUBufferUsage::eShaderStorage : {
        } break;
        case GPUBufferUsage::eStaging : {
            this->setUsagePattern(GPUBufferPattern::eDynamic)
                .addUsageFlags(vk::BufferUsageFlagBits::eTransferSrc);
        } break;
    }
    return *this;
}

void lcf::render::VulkanBufferObject::copyFromBufferWithBarriers(vk::Buffer src, uint32_t data_size_in_bytes, uint32_t src_offset_in_bytes, uint32_t dst_offset_in_bytes)
{
    auto cmd = m_context_p->getCurrentCommandBuffer();
    if (m_is_prev_write_by_staging) [[unlikely]] {
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
        cmd->pipelineBarrier2(pre_dependency);
    }
    vk::MemoryBarrier2 post_barrier;
    post_barrier.setSrcStageMask(vk::PipelineStageFlagBits2KHR::eAllTransfer)
        .setSrcAccessMask(vk::AccessFlagBits2KHR::eTransferWrite)
        .setDstStageMask(m_stage_flags)
        .setDstAccessMask(m_access_flags);
    vk::DependencyInfo post_dependency;
    post_dependency.setMemoryBarriers(post_barrier);
    vk::BufferCopy copy_region(src_offset_in_bytes, dst_offset_in_bytes, data_size_in_bytes);
    cmd->copyBuffer(src, this->getHandle(), copy_region);
    cmd->pipelineBarrier2(post_dependency);
    m_is_prev_write_by_staging = true;
}

void lcf::render::VulkanBufferObject::executeWriteSequence()
{
    if (m_timeline_semaphore_sp->isTargetReached()) {
        this->executeCpuWriteSequence();
    } else {
        this->executeGpuWriteSequence();
    }
}

void lcf::render::VulkanBufferObject::executeCpuWriteSequence()
{
    uint32_t write_required_size = m_write_segment_map.rbegin()->first.upper();
    if (write_required_size > m_size) {
        this->addWriteSegmentIfAbsent(std::span(m_buffer_sp->getMappedMemoryPtr(), m_size));
        m_size = write_required_size;
        this->create(m_context_p);
    }
    for (const auto &[interval, write_segment] : m_write_segment_map) {
        memcpy(this->getMappedMemoryPtr() + interval.lower(), write_segment.getData(), interval.upper() - interval.lower());
    }
    auto cmd = m_context_p->getCurrentCommandBuffer();
    m_timeline_semaphore_sp->increaseTargetValue();
    cmd->addSignalSubmitInfo(m_timeline_semaphore_sp->generateSubmitInfo());
}

void lcf::render::VulkanBufferObject::executeGpuWriteSequence()
{
    uint32_t write_required_size = m_write_segment_map.rbegin()->first.upper();
    if (write_required_size > m_size) {
        auto old_buffer_sp = m_buffer_sp;
        uint32_t old_size = m_size;
        m_size = write_required_size;
        this->create(m_context_p);
        this->copyFromBufferWithBarriers(old_buffer_sp->getHandle(), old_size);
    }
    uint32_t dst_offset = m_write_segment_map.begin()->first.lower();
    VulkanBufferObject::SharedPointer staging_buffer = VulkanBufferObject::makeShared();
    staging_buffer->setUsage(GPUBufferUsage::eStaging)
        .setSize(write_required_size - dst_offset)
        .create(m_context_p);
    for (const auto &[interval, write_segment] : m_write_segment_map) {
        memcpy(staging_buffer->getMappedMemoryPtr() + interval.lower() - dst_offset, write_segment.getData(), interval.upper() - interval.lower());
    }
    this->copyFromBufferWithBarriers(staging_buffer->getHandle(), staging_buffer->getSize(), 0, dst_offset);
    auto cmd = m_context_p->getCurrentCommandBuffer();
    cmd->acquireResource(staging_buffer->getBufferResource());
}

void lcf::render::VulkanBufferObject::executeGpuWriteSequenceImediate()
{
    vkutils::immediate_submit(m_context_p, [this] {
        this->executeGpuWriteSequence();
    });
}

//- BufferWriteSegment

bool BufferWriteSegment::operator==(const BufferWriteSegment &other) const noexcept
{
    return m_offset_in_bytes == other.m_offset_in_bytes
        and m_data.data() == other.m_data.data()
        and m_data.size() == other.m_data.size();
}

//! used when adding old buffer data to segment map, old buffer won't overwrite new data, thus do nothing.
BufferWriteSegment & BufferWriteSegment::operator+=(const BufferWriteSegment & other) noexcept
{
    return *this;
}

//- VulkanBufferResource

bool VulkanBufferResource::create(VulkanContext *context_p, const vk::BufferCreateInfo &buffer_info, vk::MemoryPropertyFlags memory_flags)
{
    auto memory_allocator = context_p->getMemoryAllocator();
    m_buffer = memory_allocator->createBuffer(buffer_info, memory_flags, m_mapped_memory_p);
    return m_buffer.get();
}
