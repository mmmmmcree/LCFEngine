#include "VulkanBuffer.h"
#include "VulkanContext.h"
#include "vulkan_utililtie.h"

lcf::render::VulkanBuffer::VulkanBuffer(VulkanContext *context) :
    m_context(context)
{
}

bool lcf::render::VulkanBuffer::create()
{
    auto memory_allocator = m_context->getMemoryAllocator();
    vk::BufferCreateInfo buffer_info = {{}, m_size, m_usage_flags, m_sharing_mode};
    vk::MemoryPropertyFlags memory_flags;
    switch (m_usage_pattern) {
        case UsagePattern::Dynamic : { memory_flags = vk::MemoryPropertyFlagBits::eHostVisible; } break;
        case UsagePattern::Static : { memory_flags = vk::MemoryPropertyFlagBits::eDeviceLocal; } break;
        default : { memory_flags = vk::MemoryPropertyFlagBits::eDeviceLocal; } break;
    }
    m_buffer = memory_allocator->createBuffer(buffer_info, memory_flags, m_mapped_memory_ptr);

    return this->isCreated();
}

void lcf::render::VulkanBuffer::resize(uint32_t size_in_bytes)
{
    m_size = size_in_bytes;
    if (this->isCreated()) { this->create(); }
}

void lcf::render::VulkanBuffer::setData(const void *data, uint32_t size_in_bytes)
{
    this->setSubData(data, size_in_bytes, 0);
}

void lcf::render::VulkanBuffer::setSubData(const void *data, uint32_t size_in_bytes, uint32_t offset_in_bytes)
{
    if (offset_in_bytes + size_in_bytes > m_size) {
        this->resize(offset_in_bytes + size_in_bytes);
    }
    if (not this->isCreated()) { this->create(); }
    if (m_usage_pattern == UsagePattern::Static) {
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
    vkutils::immediate_submit(m_context, [&](vk::CommandBuffer cmd) {
        cmd.copyBuffer(src.getHandle(), this->getHandle(), copy_region);
    });
}

void lcf::render::VulkanBuffer::setUsageFlags(vk::BufferUsageFlags flags)
{
    m_usage_flags = flags;
    if (m_usage_pattern == UsagePattern::Static) {
        m_usage_flags |= vk::BufferUsageFlagBits::eTransferDst;
    }
}

vk::DeviceAddress lcf::render::VulkanBuffer::getDeviceAddress() const
{
    if (not (m_usage_flags & vk::BufferUsageFlagBits::eShaderDeviceAddress)) {
        return vk::DeviceAddress{};
    }
    auto device = m_context->getDevice();
    return device.getBufferAddress(vk::BufferDeviceAddressInfo(this->getHandle()));
}

void lcf::render::VulkanBuffer::setSubDataByMap(const void *data, uint32_t size_in_bytes, uint32_t offset_in_bytes)
{
    if (not m_mapped_memory_ptr) { return; }
    memcpy(m_mapped_memory_ptr + offset_in_bytes, data, size_in_bytes);
}

void lcf::render::VulkanBuffer::setSubDataByStagingBuffer(const void *data, uint32_t size_in_bytes, uint32_t offset_in_bytes)
{
    VulkanBuffer staging_buffer(m_context);
    staging_buffer.setUsagePattern(UsagePattern::Dynamic);
    staging_buffer.setUsageFlags(vk::BufferUsageFlagBits::eTransferSrc);
    staging_buffer.setData(data, size_in_bytes);
    this->copyFrom(staging_buffer, size_in_bytes, offset_in_bytes, 0u);
}
