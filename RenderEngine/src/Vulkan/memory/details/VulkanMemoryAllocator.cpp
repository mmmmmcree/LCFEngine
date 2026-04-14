#include "Vulkan/memory/details/VulkanMemoryAllocator.h"
#include "Vulkan/memory/vulkan_memory_resources.h"
#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>
#include "log.h"

using namespace lcf::render;

VulkanMemoryAllocator::~VulkanMemoryAllocator()
{
    vmaDestroyAllocator(m_allocator);
}

std::error_code VulkanMemoryAllocator::create(vk::Instance instance, vk::PhysicalDevice physical_device, vk::Device device) noexcept
{
    VmaVulkanFunctions vulkan_functions = {
        .vkGetInstanceProcAddr = &vkGetInstanceProcAddr,
        .vkGetDeviceProcAddr = &vkGetDeviceProcAddr,
    };
    VmaAllocatorCreateInfo allocator_info = {
        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = physical_device,
        .device = device,
        .pVulkanFunctions = &vulkan_functions,
        .instance = instance,
        .vulkanApiVersion = VK_API_VERSION_1_4,
    };
    if (vmaCreateAllocator(&allocator_info, &m_allocator) != VK_SUCCESS) {
        return std::make_error_code(std::errc::invalid_argument);
    }
    return {};
}

std::unique_ptr<VulkanImage> VulkanMemoryAllocator::createImage(
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
    return std::make_unique<VulkanImage>(m_allocator, allocation, image, allocation_info.size, allocation_info.pMappedData);
}

std::unique_ptr<VulkanBuffer> VulkanMemoryAllocator::createBuffer(
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
    return std::make_unique<VulkanBuffer>(m_allocator, allocation, buffer, allocation_info.size, allocation_info.pMappedData);
}
