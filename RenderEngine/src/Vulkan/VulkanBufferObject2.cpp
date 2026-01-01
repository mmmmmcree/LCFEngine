#include "Vulkan/VulkanBufferObject2.h"
#include "Vulkan/VulkanContext.h"

using namespace lcf::render;

bool VulkanBufferObject2::create(VulkanContext *context_p, size_t size_in_bytes)
{
    m_required_size = size_in_bytes;
    return m_buffer_proxy.create(context_p, size_in_bytes) and m_writer.create(context_p);
}

VulkanBufferObject2 & VulkanBufferObject2::resize(uint64_t size_in_bytes)
{
    if (size_in_bytes <= m_required_size) { return *this; }
    m_required_size = size_in_bytes;
    return *this;
}

VulkanBufferObject2 & VulkanBufferObject2::addWriteSegment(const BufferWriteSegment &segment) noexcept
{
    m_write_segments.add(segment);
    return *this;
}

VulkanBufferObject2 & VulkanBufferObject2::addWriteSegmentIfAbsent(const BufferWriteSegment &segment) noexcept
{
    m_write_segments.addIfAbsent(segment);
    return *this;
}

void VulkanBufferObject2::commit(VulkanCommandBufferObject &cmd) noexcept
{
    m_required_size = std::max(m_required_size, m_write_segments.getUpperBoundInBytes());
    if (this->getSizeInBytes() < m_required_size) {
        this->prepareResize(cmd, m_writer);
    } else if (m_write_segments.empty()) { return; }
    m_writer.addWriteRequest(m_buffer_proxy, m_write_segments)
       .write(cmd);
    m_write_segments.clear();
}

void VulkanBufferObject2::prepareResize(VulkanCommandBufferObject & cmd, VulkanBufferWriter & writer) noexcept
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
