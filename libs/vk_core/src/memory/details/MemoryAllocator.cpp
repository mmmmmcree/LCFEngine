#include "vk_core/memory/details/MemoryAllocator.h"
#include "vk_core/memory/MemoryAllocationInfo.h"
#include "vk_core/memory/details/MemoryAllocatorCreateInfo.h"
#include "vk_core/memory/enums.h"
#include "vk_core/bootstrap/api_dispatch.h"
#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>
#include <utility>

namespace {

using namespace lcf::vkc;
using namespace lcf::vkc::details;

VmaAllocationCreateInfo to_vma_allocation_create_info(const MemoryAllocationInfo & info) noexcept;

VmaAllocatorCreateFlags to_vma_allocator_create_flags(const MemoryAllocatorCreateInfo & info) noexcept;

} // anonymous namespace

namespace lcf::vkc::details {

MemoryAllocator::~MemoryAllocator() noexcept
{
    if (m_allocator) { vmaDestroyAllocator(m_allocator); }
}

MemoryAllocator::MemoryAllocator(Self && other) noexcept :
    m_allocator(std::exchange(other.m_allocator, nullptr)),
    m_buffer_device_address_enabled(std::exchange(other.m_buffer_device_address_enabled, false))
{
}

auto MemoryAllocator::operator=(Self && other) noexcept -> Self &
{
    if (this == &other) { return *this; }
    if (m_allocator) { vmaDestroyAllocator(m_allocator); }
    m_allocator = std::exchange(other.m_allocator, nullptr);
    m_buffer_device_address_enabled = std::exchange(other.m_buffer_device_address_enabled, false);
    return *this;
}

std::error_code MemoryAllocator::create(vk::Instance instance, vk::PhysicalDevice physical_device, vk::Device device, const MemoryAllocatorCreateInfo & create_info) noexcept
{
    VmaVulkanFunctions vulkan_functions {};
    vulkan_functions.vkGetInstanceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr;
    vulkan_functions.vkGetDeviceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo allocator_info {};
    allocator_info.flags = to_vma_allocator_create_flags(create_info);
    allocator_info.physicalDevice = physical_device;
    allocator_info.device = device;
    allocator_info.preferredLargeHeapBlockSize = create_info.getPreferredLargeHeapBlockSize();
    allocator_info.pVulkanFunctions = &vulkan_functions;
    allocator_info.instance = instance;
    allocator_info.vulkanApiVersion = VK_API_VERSION_1_4;
    if (vmaCreateAllocator(&allocator_info, &m_allocator) != VK_SUCCESS) {
        return std::make_error_code(std::errc::not_enough_memory);
    }
    m_buffer_device_address_enabled = create_info.isBufferDeviceAddressEnabled();
    return {};
}

std::expected<Memory<vk::Buffer>, std::error_code> MemoryAllocator::allocateBuffer(const vk::BufferCreateInfo & buffer_info, const MemoryAllocationInfo & alloc_info) const noexcept
{
    VmaAllocationCreateInfo create_info = to_vma_allocation_create_info(alloc_info);
    VkBuffer buffer = nullptr;
    VmaAllocation allocation = nullptr;
    VkResult result = vmaCreateBuffer(
        m_allocator,
        reinterpret_cast<const VkBufferCreateInfo *>(&buffer_info),
        &create_info,
        &buffer,
        &allocation,
        nullptr);
    if (result != VK_SUCCESS) { return std::unexpected(vk::make_error_code(static_cast<vk::Result>(result))); }
    return Memory<vk::Buffer>(m_allocator, allocation, buffer);
}

std::expected<Memory<vk::Image>, std::error_code> MemoryAllocator::allocateImage(const vk::ImageCreateInfo & image_info, const MemoryAllocationInfo & alloc_info) const noexcept
{
    VmaAllocationCreateInfo create_info = to_vma_allocation_create_info(alloc_info);
    VkImage image = nullptr;
    VmaAllocation allocation = nullptr;
    VkResult result = vmaCreateImage(
        m_allocator,
        reinterpret_cast<const VkImageCreateInfo *>(&image_info),
        &create_info,
        &image,
        &allocation,
        nullptr);
    if (result != VK_SUCCESS) { return std::unexpected(vk::make_error_code(static_cast<vk::Result>(result))); }
    return Memory<vk::Image>(m_allocator, allocation, image);
}

} // namespace lcf::vkc::details

namespace {

VmaAllocationCreateInfo to_vma_allocation_create_info(const MemoryAllocationInfo & info) noexcept
{
    VmaAllocationCreateInfo create_info {};
    create_info.usage = VMA_MEMORY_USAGE_AUTO;
    switch (info.getAccess()) {
        case MemoryAccess::eDeviceLocal: break;
        case MemoryAccess::eHostSequentialWrite: {
            create_info.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        } break;
        case MemoryAccess::eHostRandom: {
            create_info.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        } break;
    }
    switch (info.getStrategy()) {
        case AllocationStrategy::eDefault: break;
        case AllocationStrategy::eMinMemory: {
            create_info.flags |= VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
        } break;
        case AllocationStrategy::eMinTime: {
            create_info.flags |= VMA_ALLOCATION_CREATE_STRATEGY_MIN_TIME_BIT;
        } break;
    }
    if (info.isDedicated()) { create_info.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT; }
    if (info.isWithinBudget()) { create_info.flags |= VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT; }
    create_info.priority = info.getPriority();
    return create_info;
}

VmaAllocatorCreateFlags to_vma_allocator_create_flags(const MemoryAllocatorCreateInfo & info) noexcept
{
    VmaAllocatorCreateFlags flags = 0;
    if (info.isBufferDeviceAddressEnabled()) { flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT; }
    if (info.isMemoryBudgetEnabled()) { flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT; }
    if (info.isMemoryPriorityEnabled()) { flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT; }
    if (info.isDeviceCoherentMemoryEnabled()) { flags |= VMA_ALLOCATOR_CREATE_AMD_DEVICE_COHERENT_MEMORY_BIT; }
    if (info.isExternallySynchronized()) { flags |= VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT; }
    return flags;
}

} // anonymous namespace