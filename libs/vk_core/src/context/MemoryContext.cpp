#include "vk_core/context/MemoryContext.h"
#include "vk_core/memory/details/MemoryAllocatorCreateInfo.h"

namespace lcf::vkc {

std::error_code MemoryContext::create(
    vk::Instance instance, vk::PhysicalDevice physical_device, vk::Device device,
    const details::MemoryAllocatorCreateInfo &create_info) noexcept
{
    return m_allocator.create(instance, physical_device, device, create_info);
}

std::expected<Buffer, std::error_code> MemoryContext::createBuffer(const vk::BufferCreateInfo &buffer_info, const MemoryAllocationInfo &alloc_info) const noexcept
{
    auto expected_memory = m_allocator.allocateBuffer(buffer_info, alloc_info);
    if (not expected_memory) { return std::unexpected {expected_memory.error()}; }
    vk::DeviceAddress device_address = 0;
    if (m_allocator.isBufferDeviceAddressEnabled() and (buffer_info.usage & vk::BufferUsageFlagBits::eShaderDeviceAddress)) {
        device_address = m_device.getBufferAddress({expected_memory->handle()});
    }
    return Buffer {std::move(*expected_memory), device_address};
}

std::expected<Image, std::error_code> MemoryContext::createImage(const vk::ImageCreateInfo &image_info, const MemoryAllocationInfo &alloc_info) const noexcept
{
    auto expected_memory = m_allocator.allocateImage(image_info, alloc_info);
    if (not expected_memory) { return std::unexpected {expected_memory.error()}; }
    return Image {std::move(*expected_memory)};
}

}