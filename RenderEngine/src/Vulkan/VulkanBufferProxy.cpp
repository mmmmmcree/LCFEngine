#include "Vulkan/VulkanBufferProxy.h"
#include "Vulkan/VulkanContext.h"
#include "log.h"

using namespace lcf::render;

bool VulkanBufferProxy::create(
    VulkanContext *context_p,
    const vk::BufferCreateInfo & buffer_info,
    const MemoryAllocationCreateInfo & memory_info)
{
    auto device = context_p->getDevice();
    auto & memory_allocator = context_p->getMemoryAllocator();
    m_buffer_sp = VulkanBuffer::makeShared();
    if (not m_buffer_sp->create(memory_allocator, buffer_info, memory_info)) {
        lcf_log_error("Failed to create buffer");
        return false;
    }
    if (buffer_info.usage & vk::BufferUsageFlagBits::eShaderDeviceAddress) {
        m_device_address = device.getBufferAddress(m_buffer_sp->getHandle());
    }
    return true;
}