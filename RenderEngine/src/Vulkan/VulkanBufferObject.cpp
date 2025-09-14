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
    m_timeline_semaphore.create(m_context_p);
    
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
            m_write_segment_method = &Self::writeSegments;
        } break;
        case GPUBufferPattern::eStatic : {
            memory_flags = vk::MemoryPropertyFlagBits::eDeviceLocal;
            m_write_segment_method = &Self::writeSegmentsByStagingImediate;
        } break;
    }
    m_buffer_sp = VulkanBufferResource::makeShared();
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
    /*
        
    */
    return *this;
}

void VulkanBufferObject::addWriteSegment(const BufferWriteSegment &segment) noexcept
{
    m_write_segments.emplace(segment);
}

void VulkanBufferObject::commitWriteSegments()
{
    WriteSegmentList segments(m_write_segments.size());
    while (not m_write_segments.empty()) {
        // TODO: remove redundant segments
        segments.emplace_back(m_write_segments.top());
        m_write_segments.pop();
    }
    //todo check if segments out of range, call resize if necessary
    (this->*m_write_segment_method)(segments);
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

void VulkanBufferObject::writeSegments(const VulkanBufferObject::WriteSegmentList &segments)
{
    if (m_timeline_semaphore.isTargetReached()) {
        this->writeSegmentsByMapping(segments);
        auto cmd = m_context_p->getCurrentCommandBuffer();
        m_timeline_semaphore.increaseTargetValue();
        cmd->addSignalSubmitInfo(m_timeline_semaphore.generateSubmitInfo());
    } else {
        this->writeSegmentsByStaging(segments);
    }
}

void lcf::render::VulkanBufferObject::writeSegmentsByMapping(const WriteSegmentList &segments)
{
    for (const auto &segment : segments) {
        memcpy(this->getMappedMemoryPtr() + segment.offset_in_bytes, segment.data.data(), segment.data.size_bytes());
    }
    m_is_prev_write_by_staging = false;
}

void VulkanBufferObject::writeSegmentsByStaging(const VulkanBufferObject::WriteSegmentList &segments)
{
    if (segments.empty()) { return; }
    auto cmd = m_context_p->getCurrentCommandBuffer();
    VulkanBufferObject::SharedPointer staging_buffer = VulkanBufferObject::makeShared();
    const auto &last_segment = segments.back();
    uint32_t staging_size = last_segment.offset_in_bytes + last_segment.data.size_bytes();
    staging_buffer->setUsage(GPUBufferUsage::eStaging)
        .setSize(staging_size)
        .create(m_context_p);
    staging_buffer->writeSegmentsByMapping(segments);

    if (m_is_prev_write_by_staging) {
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
    vk::BufferCopy copy_region(0u, 0u, staging_buffer->getSize());
    cmd->copyBuffer(staging_buffer->getHandle(), this->getHandle(), copy_region);
    cmd->pipelineBarrier2(post_dependency);
    cmd->acquireResource(staging_buffer->getBufferResource());
    m_is_prev_write_by_staging = true;
}

void VulkanBufferObject::writeSegmentsByStagingImediate(const WriteSegmentList &segments)
{
    vkutils::immediate_submit(m_context_p, [this, &segments] {
        this->writeSegmentsByStaging(segments);
    });
}

//- VulkanBufferResource

bool VulkanBufferResource::create(VulkanContext *context_p, const vk::BufferCreateInfo &buffer_info, vk::MemoryPropertyFlags memory_flags)
{
    auto memory_allocator = context_p->getMemoryAllocator();
    m_buffer = memory_allocator->createBuffer(buffer_info, memory_flags, m_mapped_memory_p);
    return m_buffer.get();
}
