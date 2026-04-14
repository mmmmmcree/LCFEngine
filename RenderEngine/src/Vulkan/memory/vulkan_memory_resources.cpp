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

VulkanBuffer::~VulkanBuffer() noexcept
{
    vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);
}

VulkanBuffer::VulkanBuffer(Self && other) noexcept :
    m_allocator(std::exchange(other.m_allocator, nullptr)),
    m_allocation(std::exchange(other.m_allocation, nullptr)),
    m_buffer(std::exchange(other.m_buffer, nullptr)),
    m_mapped_data_p(std::exchange(other.m_mapped_data_p, nullptr)),
    m_size(std::exchange(other.m_size, 0))
{
}

VulkanBuffer & VulkanBuffer::operator=(Self && other) noexcept
{
    if (this == &other) { return *this; }
    m_allocator = std::exchange(other.m_allocator, nullptr);
    m_allocation = std::exchange(other.m_allocation, nullptr);
    m_buffer = std::exchange(other.m_buffer, nullptr);
    m_mapped_data_p = std::exchange(other.m_mapped_data_p, nullptr);
    m_size = std::exchange(other.m_size, 0);
    return *this;
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

VulkanImage::VulkanImage(vk::Image image) :
    m_image(image)
{
}

VulkanImage::~VulkanImage() noexcept
{
    if (m_allocator and m_allocation) {
        vmaDestroyImage(m_allocator, m_image, m_allocation);
    }
}

VulkanImage::VulkanImage(Self && other) noexcept :
    m_allocator(std::exchange(other.m_allocator, nullptr)),
    m_allocation(std::exchange(other.m_allocation, nullptr)),
    m_image(std::exchange(other.m_image, nullptr)),
    m_mapped_data_p(std::exchange(other.m_mapped_data_p, nullptr)),
    m_size(std::exchange(other.m_size, 0))
{
}

VulkanImage & VulkanImage::operator=(Self && other) noexcept
{
    if (this == &other) { return *this; }
    m_allocator = std::exchange(other.m_allocator, nullptr);
    m_allocation = std::exchange(other.m_allocation, nullptr);
    m_image = std::exchange(other.m_image, nullptr);
    m_mapped_data_p = std::exchange(other.m_mapped_data_p, nullptr);
    m_size = std::exchange(other.m_size, 0);
    return *this;
}

vk::Result VulkanImage::flush(VkDeviceSize offset, VkDeviceSize size)
{
    return static_cast<vk::Result>(vmaFlushAllocation(m_allocator, m_allocation, offset, size));
}
