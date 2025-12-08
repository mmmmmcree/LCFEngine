#include "Vulkan/VulkanBufferResource.h"
#include "Vulkan/VulkanContext.h"

using namespace lcf::render;

bool VulkanBufferResource::create(const VulkanMemoryAllocator & allocator, const vk::BufferCreateInfo &buffer_info, const MemoryAllocationCreateInfo &memory_allocation_info)
{
    m_buffer_up = allocator.createBuffer(buffer_info, memory_allocation_info);
    return m_buffer_up.get();
}
