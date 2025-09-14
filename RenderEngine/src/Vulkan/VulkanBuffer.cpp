#include "VulkanBuffer.h"
#include "VulkanContext.h"
#include "vulkan_utililtie.h"
#include <boost/align.hpp>

lcf::render::VulkanBuffer::VulkanBuffer(VulkanContext *context) :
    m_context_p(context)
{
}

bool lcf::render::VulkanBuffer::create()
{
    auto memory_allocator = m_context_p->getMemoryAllocator();
    vk::BufferCreateInfo buffer_info = {{}, m_size, m_usage_flags, m_sharing_mode};
    vk::MemoryPropertyFlags memory_flags;
    switch (m_usage_pattern) {
        case UsagePattern::eDynamic : { memory_flags = vk::MemoryPropertyFlagBits::eHostVisible; } break;
        case UsagePattern::eStatic : { memory_flags = vk::MemoryPropertyFlagBits::eDeviceLocal; } break;
        default : { memory_flags = vk::MemoryPropertyFlagBits::eDeviceLocal; } break;
    }
    m_buffer = memory_allocator->createBuffer(buffer_info, memory_flags, m_mapped_memory_ptr);
    return this->isCreated();
}

void lcf::render::VulkanBuffer::setSize(uint32_t size_in_bytes)
{
    uint32_t alignment = 4;
    vk::PhysicalDeviceLimits limits = m_context_p->getPhysicalDevice().getProperties().limits;
    if (m_usage_flags & vk::BufferUsageFlagBits::eUniformBuffer) {
        alignment = limits.minUniformBufferOffsetAlignment;
    } else if (m_usage_flags & vk::BufferUsageFlagBits::eStorageBuffer) {
        alignment = limits.minStorageBufferOffsetAlignment;
    } else if (m_usage_flags & (vk::BufferUsageFlagBits::eStorageTexelBuffer|vk::BufferUsageFlagBits::eUniformTexelBuffer)) {
        alignment = limits.minTexelBufferOffsetAlignment;
    }
    m_size = boost::alignment::align_up(size_in_bytes, alignment);
}

void lcf::render::VulkanBuffer::resize(uint32_t size_in_bytes)
{
    this->setSize(size_in_bytes); 
    if (this->isCreated()) { this->create(); }
}

void lcf::render::VulkanBuffer::setData(const void *data, uint32_t size_in_bytes)
{
    this->setSubData(data, size_in_bytes, 0);
}

void lcf::render::VulkanBuffer::setSubData(const void *data, uint32_t size_in_bytes, uint32_t offset_in_bytes)
{
    uint32_t required_size = offset_in_bytes + size_in_bytes;
    if (required_size > m_size) { this->resize(required_size); }
    if (not this->isCreated()) { this->create(); }
    if (m_usage_pattern == UsagePattern::eStatic) {
        this->setSubDataByStagingBuffer(data, size_in_bytes, offset_in_bytes);
    } else {
        this->setSubDataByMap(data, size_in_bytes, offset_in_bytes);
    }
}

void lcf::render::VulkanBuffer::copyFrom(const VulkanBuffer &src, uint32_t data_size_in_bytes, uint32_t src_offset_in_bytes, uint32_t dst_offset_in_bytes)
{
    if (not this->isCreated()) {
        qDebug() << "lcf::VulkanBuffer::copyFrom() : This buffer is not created";
        return;
    }
    if (not src.isCreated()) {
        qDebug() << "lcf::VulkanBuffer::copyFrom() : Other buffer is not created";
        return;
    }
    if (not this->hasUsageFlagsBits(vk::BufferUsageFlagBits::eTransferDst)) {
        qDebug() << "lcf::VulkanBuffer::copyFrom() : This buffer does not have eTransferDst usage flag";
        return;
    }
    if (not src.hasUsageFlagsBits(vk::BufferUsageFlagBits::eTransferSrc)) {
        qDebug() << "lcf::VulkanBuffer::copyFrom() : Other buffer does not have eTransferSrc usage flag";
        return;
    }
    if (data_size_in_bytes == 0u) { data_size_in_bytes = src.getSize() - src_offset_in_bytes; }
    vk::BufferCopy copy_region(src_offset_in_bytes, dst_offset_in_bytes, data_size_in_bytes);
    auto cmd = m_context_p->getCurrentCommandBuffer();
    vkutils::immediate_submit(m_context_p, [&] {
        auto cmd = m_context_p->getCurrentCommandBuffer();
        cmd->copyBuffer(src.getHandle(), this->getHandle(), copy_region);
    });
}

void lcf::render::VulkanBuffer::setUsageFlags(vk::BufferUsageFlags flags)
{
    m_usage_flags = flags;
    if (m_usage_pattern == UsagePattern::eStatic) {
        m_usage_flags |= vk::BufferUsageFlagBits::eTransferDst;
    }
}

vk::DeviceAddress lcf::render::VulkanBuffer::getDeviceAddress() const
{
    if (not (m_usage_flags & vk::BufferUsageFlagBits::eShaderDeviceAddress)) {
        return vk::DeviceAddress{};
    }
    auto device = m_context_p->getDevice();
    return device.getBufferAddress(vk::BufferDeviceAddressInfo(this->getHandle()));
}

void lcf::render::VulkanBuffer::setSubDataByMap(const void *data, uint32_t size_in_bytes, uint32_t offset_in_bytes)
{
    if (not m_mapped_memory_ptr) { return; }
    memcpy(m_mapped_memory_ptr + offset_in_bytes, data, size_in_bytes);
}

void lcf::render::VulkanBuffer::setSubDataByStagingBuffer(const void *data, uint32_t size_in_bytes, uint32_t offset_in_bytes)
{
    VulkanBuffer staging_buffer(m_context_p);
    staging_buffer.setUsagePattern(UsagePattern::eDynamic);
    staging_buffer.setUsageFlags(vk::BufferUsageFlagBits::eTransferSrc);
    staging_buffer.setData(data, size_in_bytes);
    this->copyFrom(staging_buffer, size_in_bytes, 0u, offset_in_bytes);
}


lcf::render::VulkanDynamicMultiBuffer::VulkanDynamicMultiBuffer(VulkanContext *context) :
    m_context_p(context)
{
    m_buffers.emplace_back(VulkanBuffer::makeUnique(m_context_p));
}

void lcf::render::VulkanDynamicMultiBuffer::setBufferCount(uint32_t count)
{
    const auto *buffer_prototype = m_buffers[0].get(); //! must be pointer, not reference to avoid memory tranfer
    for (size_t i = m_buffers.size(); i < count; ++i) {
        auto &new_buffer = m_buffers.emplace_back(VulkanBuffer::makeUnique(m_context_p));
        new_buffer->setUsageFlags(buffer_prototype->getUsageFlags());
        new_buffer->setUsagePattern(buffer_prototype->getUsagePattern());
    }
}

void lcf::render::VulkanDynamicMultiBuffer::setUsageFlags(vk::BufferUsageFlags flags)
{
    for (auto &buffer : m_buffers) {
        buffer->setUsageFlags(flags);
    }
}

void lcf::render::VulkanDynamicMultiBuffer::setUsagePattern(GPUBuffer::UsagePattern pattern)
{
    for (auto &buffer : m_buffers) {
        buffer->setUsagePattern(pattern);
    }
}

void lcf::render::VulkanDynamicMultiBuffer::setData(const void *data, uint32_t size_in_bytes)
{
    m_buffers[m_current_index]->setData(data, size_in_bytes);
}

void lcf::render::VulkanDynamicMultiBuffer::setSubData(const void *data, uint32_t size_in_bytes, uint32_t offset_in_bytes)
{
    m_buffers[m_current_index]->setSubData(data, size_in_bytes, offset_in_bytes);
}

lcf::render::VulkanStaticMultiBuffer::VulkanStaticMultiBuffer(VulkanContext *context) :
    VulkanBuffer(context)
{
}

bool lcf::render::VulkanStaticMultiBuffer::create()
{
    m_size = m_single_buffer_size * m_buffer_count;
    return VulkanBuffer::create();
}

void lcf::render::VulkanStaticMultiBuffer::setSingleBufferSize(uint32_t size_in_bytes)
{
    this->setSize(size_in_bytes);
    m_single_buffer_size = m_size;
}

void lcf::render::VulkanStaticMultiBuffer::setData(const void *data, uint32_t size_in_bytes)
{
    VulkanBuffer::setSubData(data, size_in_bytes, this->getDynamicOffset());
}

void lcf::render::VulkanStaticMultiBuffer::setSubData(const void *data, uint32_t size_in_bytes, uint32_t offset_in_bytes)
{
    VulkanBuffer::setSubData(data, size_in_bytes, this->getDynamicOffset() + offset_in_bytes);
}


// ---VulkanTimelineBuffer---

lcf::render::VulkanTimelineBuffer::VulkanTimelineBuffer(VulkanContext * context) :
    VulkanBuffer(context)
{
    this->setUsagePattern(UsagePattern::eDynamic);
    m_timeline_semaphore.create(m_context_p);
}


void lcf::render::VulkanTimelineBuffer::setData(const void *data, uint32_t size_in_bytes)
{
    this->setSubData(data, size_in_bytes, 0u);
}

void lcf::render::VulkanTimelineBuffer::setSubData(const void *data, uint32_t size_in_bytes, uint32_t offset_in_bytes)
{
    if (not m_is_writing) { return; }
    uint32_t required_size = offset_in_bytes + size_in_bytes;
    if (required_size > m_size) { this->resize(required_size); }
    if (not this->isCreated()) { this->create(); }
    m_current_writing_buffer->setSubDataByMap(data, size_in_bytes, offset_in_bytes);
}

void lcf::render::VulkanTimelineBuffer::beginWrite()
{
    m_is_writing = true;
    if (not this->isPendingComplete()) {
        m_current_writing_buffer = this;
        return;
    }
    while (not m_staging_buffers.empty()) {
        auto &staging_buffer = m_staging_buffers.front();
        bool is_reusable = staging_buffer->isPendingComplete() and staging_buffer->getSize() >= m_size;
        if (not is_reusable) { continue; }
        m_current_writing_buffer = staging_buffer.get();
        return;
    }
    auto &staging_buffer = m_staging_buffers.emplace(VulkanTimelineBuffer::makeUnique(m_context_p));
    staging_buffer->setSize(m_size);
    staging_buffer->setUsagePattern(UsagePattern::eDynamic);
    staging_buffer->setUsageFlags(vk::BufferUsageFlagBits::eTransferSrc);
    staging_buffer->create();
    m_current_writing_buffer = staging_buffer.get();
}

void lcf::render::VulkanTimelineBuffer::endWrite()
{
    if (not m_is_writing) { return; }
    m_is_writing = false;
    m_current_writing_buffer->m_timeline_semaphore.increaseTargetValue();
    if (not this->isUsingStagingBufferForWrite()) {
        return;
    }
    if (m_current_writing_buffer->getSize() > m_size) { this->resize(m_current_writing_buffer->getSize()); }
    if (this->isPendingComplete()) {
        memcpy(m_mapped_memory_ptr, m_current_writing_buffer->m_mapped_memory_ptr, m_size);
        m_timeline_semaphore.increaseTargetValue();
    } else {
        vk::BufferCopy copy_region(0u, 0u, m_size);
        auto cmd = m_context_p->getCurrentCommandBuffer();
        cmd->copyBuffer(m_current_writing_buffer->getHandle(), this->getHandle(), copy_region);
        
        vk::MemoryBarrier2 barrier;
        barrier.setSrcStageMask(vk::PipelineStageFlagBits2KHR::eAllTransfer)
            .setSrcAccessMask(vk::AccessFlagBits2KHR::eTransferWrite)
            .setDstStageMask(vk::PipelineStageFlagBits2KHR::eAllGraphics)
            .setDstAccessMask(vk::AccessFlagBits2KHR::eShaderRead | vk::AccessFlagBits2KHR::eUniformRead);
        vk::DependencyInfo dependency_info;
        dependency_info.setMemoryBarriers(barrier);
        cmd->pipelineBarrier2(dependency_info);
    }
    while (not m_staging_buffers.empty()) {
        auto &staging_buffer = m_staging_buffers.front();
        if (not staging_buffer->isPendingComplete()) { return; }
        m_staging_buffers.pop();
    }
}

vk::SemaphoreSubmitInfo lcf::render::VulkanTimelineBuffer::generateSubmitInfo() const
{
    if (not m_current_writing_buffer) { return {}; }
    return m_current_writing_buffer->m_timeline_semaphore.generateSubmitInfo();
}