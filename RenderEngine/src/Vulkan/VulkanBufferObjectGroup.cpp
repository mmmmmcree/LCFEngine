#include "Vulkan/VulkanBufferObjectGroup.h"
#include "Vulkan/VulkanCommandBufferObject.h"

using namespace lcf::render;

bool VulkanBufferObjectGroup::create(VulkanContext *context_p, GPUBufferPattern pattern)
{
    m_context_p = context_p;
    return m_buffer_writer.setPattern(pattern).create(m_context_p);
}

void VulkanBufferObjectGroup::emplace(uint64_t size, GPUBufferUsage buffer_usage)
{
    auto & buffer_object = m_buffer_object_list.emplace_back();
    buffer_object.setPattern(m_buffer_writer.getPattern())
        .setUsage(buffer_usage)
        .create(m_context_p, size);
}

void VulkanBufferObjectGroup::commitAll(VulkanCommandBufferObject & cmd) noexcept
{
    for (auto & buffer_object : m_buffer_object_list) {
        auto & required_size = buffer_object.m_required_size;
        auto & write_segments = buffer_object.m_write_segments;
        auto & buffer_proxy = buffer_object.m_buffer_proxy;
        required_size = std::max(required_size, write_segments.getUpperBoundInBytes());
        if (buffer_object.getSizeInBytes() < required_size) {
            buffer_object.prepareResize(cmd, m_buffer_writer);
        } else if (write_segments.empty()) { continue; }
        m_buffer_writer.addWriteRequest(buffer_proxy, write_segments);
    }
    m_buffer_writer.write(cmd);
    for (auto & buffer_object : m_buffer_object_list) {
        buffer_object.m_write_segments.clear();
    }
}
