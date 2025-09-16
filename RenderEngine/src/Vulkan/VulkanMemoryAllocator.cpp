#include "VulkanMemoryAllocator.h"
#include "VulkanContext.h"
#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>
#include "error.h"

using namespace lcf::render;

lcf::render::VulkanMemoryAllocator::~VulkanMemoryAllocator()
{
    vmaDestroyAllocator(m_allocator);
}

bool lcf::render::VulkanMemoryAllocator::create(VulkanContext * context)
{
    VmaVulkanFunctions vulkan_functions = {
        .vkGetInstanceProcAddr = &vkGetInstanceProcAddr,
        .vkGetDeviceProcAddr = &vkGetDeviceProcAddr,
    };
    VmaAllocatorCreateInfo allocator_info = {
        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = context->getPhysicalDevice(),
        .device = context->getDevice(),
        .pVulkanFunctions = &vulkan_functions,
        .instance = context->getInstance(),
        .vulkanApiVersion = VK_API_VERSION_1_3,
    };
    return vmaCreateAllocator(&allocator_info, &m_allocator) == VK_SUCCESS;
}

lcf::render::VMAUniqueImage lcf::render::VulkanMemoryAllocator::createImage(const vk::ImageCreateInfo &image_info)
{
    VmaAllocationCreateInfo alloc_info = {
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    };

    VkImage image = nullptr;
    VmaAllocation allocation = nullptr;
    VkResult result = vmaCreateImage(
        m_allocator,
        reinterpret_cast<const VkImageCreateInfo *>(&image_info),
        &alloc_info,
        &image,
        &allocation,
        nullptr
    );
    if (result != VK_SUCCESS) {
        LCF_THROW_RUNTIME_ERROR("lcf::render::VulkanMemoryAllocator::createImage: Failed to create image with VMA.");
    }
    return VMAUniqueImage(image, VMAImageDeleter(m_allocator, allocation));
}

lcf::render::VMAUniqueBuffer lcf::render::VulkanMemoryAllocator::createBuffer(const vk::BufferCreateInfo &buffer_info, vk::MemoryPropertyFlags memory_flags, std::byte * & mapped_data)
{
    VmaAllocationCreateInfo alloc_info = {
        .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .requiredFlags = static_cast<VkMemoryPropertyFlags>(memory_flags)
    };
    VkBuffer buffer = nullptr;
    VmaAllocation allocation = nullptr;
    VmaAllocationInfo allocation_info = {};
    VkResult result = vmaCreateBuffer(
        m_allocator,
        reinterpret_cast<const VkBufferCreateInfo *>(&buffer_info),
        &alloc_info,
        &buffer,
        &allocation,
        &allocation_info
    );
    if (result != VK_SUCCESS) {
        LCF_THROW_RUNTIME_ERROR("lcf::render::VulkanMemoryAllocator::createBuffer: Failed to create buffer with VMA.");
    }
    mapped_data = static_cast<std::byte *>(allocation_info.pMappedData);
    return VMAUniqueBuffer(buffer, VMABufferDeleter(m_allocator, allocation));
}

VMABuffer::UniquePointer VulkanMemoryAllocator::createBuffer(const vk::BufferCreateInfo &buffer_info, const MemoryAllocationCreateInfo &mem_alloc_info)
{
    VmaAllocationCreateInfo alloc_create_info = {
        .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .requiredFlags = static_cast<VkMemoryPropertyFlags>(mem_alloc_info.memory_flags)
    };
    VkBuffer buffer = nullptr;
    VmaAllocation allocation = nullptr;
    VmaAllocationInfo allocation_info = {};
    VkResult result = vmaCreateBuffer(
        m_allocator,
        reinterpret_cast<const VkBufferCreateInfo *>(&buffer_info),
        &alloc_create_info,
        &buffer,
        &allocation,
        &allocation_info
    );
    if (result != VK_SUCCESS) {
        LCF_THROW_RUNTIME_ERROR("lcf::render::VulkanMemoryAllocator::createBuffer: Failed to create buffer with VMA.");
    }
    return VMABuffer::makeUnique(m_allocator, allocation, buffer, allocation_info.size, allocation_info.pMappedData);
}

//- Deleters

lcf::render::VMABufferDeleter::VMABufferDeleter(VmaAllocator_T *allocator, VmaAllocation_T *allocation) :
    m_allocator(allocator),
    m_allocation(allocation)
{
}

void lcf::render::VMABufferDeleter::operator()(VkBuffer buffer) const noexcept
{
    vmaDestroyBuffer(m_allocator, buffer, m_allocation);
}

lcf::render::VMAImageDeleter::VMAImageDeleter(VmaAllocator_T *allocator, VmaAllocation_T *allocation) :
    m_allocator(allocator),
    m_allocation(allocation)
{
}

void lcf::render::VMAImageDeleter::operator()(VkImage image) const noexcept
{
    vmaDestroyImage(m_allocator, image, m_allocation);
}

lcf::render::VMABuffer::VMABuffer(VmaAllocator_T *allocator, VmaAllocation_T *allocation, vk::Buffer buffer, vk::DeviceSize size, void *mapped_data_p) :
    m_allocator(allocator),
    m_allocation(allocation),
    m_buffer(buffer),
    m_size(size),
    m_mapped_data_p(static_cast<std::byte *>(mapped_data_p))
{
}

lcf::render::VMABuffer::~VMABuffer()
{
    vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);
}

vk::Result lcf::render::VMABuffer::flush(VkDeviceSize offset, VkDeviceSize size)
{
    return static_cast<vk::Result>(vmaFlushAllocation(m_allocator, m_allocation, offset, size));
}
