#include "Vulkan/vulkan_memory_resources.h"

using namespace lcf::render;

VulkanBuffer::VulkanBuffer(VmaAllocator allocator, VmaAllocation allocation, vk::Buffer buffer, vk::DeviceSize size, void *mapped_data_p) :
    m_allocator(allocator),
    m_allocation(allocation),
    m_buffer(buffer),
    m_size(size),
    m_mapped_data_p(static_cast<std::byte *>(mapped_data_p))
{
}

VulkanBuffer::~VulkanBuffer()
{
    vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);
}

vk::Result VulkanBuffer::flush(VkDeviceSize offset, VkDeviceSize size)
{
    return static_cast<vk::Result>(vmaFlushAllocation(m_allocator, m_allocation, offset, size));
}

VulkanImage::VulkanImage(VmaAllocator allocator, VmaAllocation allocation, vk::Image image, vk::DeviceSize size, void * mapped_data_p) :
    m_allocator(allocator),
    m_allocation(allocation),
    m_image(image),
    m_size(size),
    m_mapped_data_p(static_cast<std::byte *>(mapped_data_p))
{
}

VulkanImage::~VulkanImage()
{
    vmaDestroyImage(m_allocator, m_image, m_allocation);
}

vk::Result VulkanImage::flush(VkDeviceSize offset, VkDeviceSize size)
{
    return static_cast<vk::Result>(vmaFlushAllocation(m_allocator, m_allocation, offset, size));
}
