#include "Vulkan/VulkanMemoryAllocator.h"
#include "Vulkan/VulkanContext.h"
#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>
#include "log.h"

using namespace lcf::render;

VulkanMemoryAllocator::~VulkanMemoryAllocator()
{
    vmaDestroyAllocator(m_allocator);
}

bool VulkanMemoryAllocator::create(VulkanContext * context)
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

VulkanImage::UniquePointer VulkanMemoryAllocator::createImage(
    const vk::ImageCreateInfo &image_info,
    const MemoryAllocationCreateInfo &mem_alloc_info) const
{
    VmaAllocationCreateInfo alloc_create_info = {
        .requiredFlags = static_cast<VkMemoryPropertyFlags>(mem_alloc_info.memory_flags)
    };
    if (mem_alloc_info.memory_flags & vk::MemoryPropertyFlagBits::eHostVisible) {
        alloc_create_info.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT ;
    }
    VkImage image = nullptr;
    VmaAllocation allocation = nullptr;
    VmaAllocationInfo allocation_info = {};
    VkResult result = vmaCreateImage(
        m_allocator,
        reinterpret_cast<const VkImageCreateInfo *>(&image_info),
        &alloc_create_info,
        &image,
        &allocation,
        &allocation_info
    );
    if (result != VK_SUCCESS) {
        std::runtime_error error("Failed to create image with VMA.");
        lcf_log_error(error.what());
        throw error;
    }
    return VulkanImage::makeUnique(m_allocator, allocation, image, allocation_info.size, allocation_info.pMappedData);
}

VulkanBuffer::UniquePointer VulkanMemoryAllocator::createBuffer(
    const vk::BufferCreateInfo &buffer_info,
    const MemoryAllocationCreateInfo &mem_alloc_info) const
{
    VmaAllocationCreateInfo alloc_create_info = {
        .requiredFlags = static_cast<VkMemoryPropertyFlags>(mem_alloc_info.memory_flags)
    };
    if (mem_alloc_info.memory_flags & vk::MemoryPropertyFlagBits::eHostVisible) {
        alloc_create_info.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT ;
    }
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
        std::runtime_error error("Failed to create buffer with VMA.");
        lcf_log_error(error.what());
        throw error;
    }
    return VulkanBuffer::makeUnique(m_allocator, allocation, buffer, allocation_info.size, allocation_info.pMappedData);
}
