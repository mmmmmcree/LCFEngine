#include "VulkanBufferResource.h"
#include "VulkanContext.h"

using namespace lcf::render;

bool VulkanBufferResource::create(VulkanMemoryAllocator & allocator, const vk::BufferCreateInfo &buffer_info, const MemoryAllocationCreateInfo &memory_allocation_info)
{
    m_buffer_up = allocator.createBuffer(buffer_info, memory_allocation_info);
    return m_buffer_up.get();
}
