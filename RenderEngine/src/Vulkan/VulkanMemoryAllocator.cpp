#include "VulkanMemoryAllocator.h"
#include "VulkanContext.h"
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

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
        const char * error_message = "lcf::render::VulkanMemoryAllocator::createImage: Failed to create image with VMA.";
        qDebug() << error_message;
        throw std::runtime_error(error_message);
    }
    return VMAUniqueImage(image, [=](VkImage image) { vmaDestroyImage(m_allocator, image, allocation); });
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
        const char * error_message = "lcf::render::VulkanMemoryAllocator::createBuffer: Failed to create buffer with VMA.";
        qDebug() << error_message;
        throw std::runtime_error(error_message);
    }
    mapped_data = static_cast<std::byte *>(allocation_info.pMappedData);
    return VMAUniqueBuffer(buffer, [=](VkBuffer buffer) { vmaDestroyBuffer(m_allocator, buffer, allocation); });
}
