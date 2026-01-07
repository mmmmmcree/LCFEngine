#include "Vulkan/VulkanBufferObject.h"
#include "Vulkan/VulkanContext.h"
#include "Vulkan/VulkanCommandBufferObject.h"

using namespace lcf::render;

bool VulkanBufferObject::create(VulkanContext *context_p, size_t size_in_bytes)
{
    m_required_size = size_in_bytes;
    return m_buffer_proxy.create(context_p, size_in_bytes) and m_writer.create(context_p);
}

VulkanBufferObject & VulkanBufferObject::setPattern(GPUBufferPattern pattern) noexcept
{
    m_writer.setPattern(pattern);
    m_buffer_proxy.setPattern(pattern);
    return *this;
}

VulkanBufferObject & VulkanBufferObject::resize(uint64_t size_in_bytes)
{
    if (size_in_bytes <= m_required_size) { return *this; }
    m_required_size = size_in_bytes;
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

void VulkanBufferObject::commit(VulkanCommandBufferObject &cmd) noexcept
{
    m_required_size = std::max(m_required_size, m_write_segments.getUpperBoundInBytes());
    if (this->getSizeInBytes() < m_required_size) {
        this->prepareResize(cmd, m_writer);
    } else if (m_write_segments.empty()) { return; }
    m_writer.addWriteRequest(m_buffer_proxy, m_write_segments)
       .write(cmd);
    m_write_segments.clear();
}

void VulkanBufferObject::prepareResize(VulkanCommandBufferObject & cmd, VulkanBufferWriter & writer) noexcept
{
    const auto & old_buffer_resource = m_buffer_proxy.getResource();
    uint32_t old_size = m_buffer_proxy.getSizeInBytes();
    m_buffer_proxy.recreate(m_required_size);
    if (writer.hasPendingOperations()) {
        writer.copyFromBufferWithBarriers(cmd,
            old_buffer_resource->getHandle(), m_buffer_proxy,
            old_size,
            0, 0);
    } else {
        this->addWriteSegmentIfAbsent({as_bytes_from_ptr(old_buffer_resource->getMappedMemoryPtr(), old_size)});
    }
    cmd.acquireResource(old_buffer_resource);
}
