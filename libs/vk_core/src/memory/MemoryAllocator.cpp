#include "vk_core/memory/MemoryAllocator.h"
#include "vk_core/memory/info_structs.h"
#include "vk_core/memory/details/Allocator.h"

namespace lcf::vkc {

MemoryAllocator::~MemoryAllocator() noexcept = default;

MemoryAllocator::MemoryAllocator() noexcept = default;

MemoryAllocator::MemoryAllocator(Self &&) noexcept = default;

auto MemoryAllocator::operator =(Self &&) noexcept -> Self & = default;

std::error_code MemoryAllocator::create(
    vk::Instance instance, vk::PhysicalDevice physical_device, vk::Device device,
    const MemoryAllocatorCreateInfo &create_info) noexcept
{
    m_device = device;
    m_bda_enabled = create_info.isBufferDeviceAddressEnabled();
    m_allocator_up = std::make_unique<details::VMAllocator>();
    return m_allocator_up->create(instance, physical_device, device, create_info);
}

std::expected<details::Memory<vk::Buffer>, std::error_code> MemoryAllocator::allocateBuffer(
    const vk::BufferCreateInfo & buffer_info, const MemoryAllocationInfo & alloc_info) const noexcept
{
    return m_allocator_up->allocateBuffer(buffer_info, alloc_info);
}

std::expected<details::Memory<vk::Image>, std::error_code> MemoryAllocator::allocateImage(
    const vk::ImageCreateInfo & image_info, const MemoryAllocationInfo & alloc_info) const noexcept
{
    return m_allocator_up->allocateImage(image_info, alloc_info);
}

}