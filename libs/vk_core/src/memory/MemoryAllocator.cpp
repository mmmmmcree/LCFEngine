#include "vk_core/memory/MemoryAllocator.h"
#include "vk_core/memory/info_structs.h"

namespace lcf::vkc {

std::error_code MemoryAllocator::create(
    vk::Instance instance, vk::PhysicalDevice physical_device, vk::Device device,
    const MemoryAllocatorCreateInfo &create_info) noexcept
{
    m_bda_enabled = create_info.isBufferDeviceAddressEnabled();
    return m_allocator.create(instance, physical_device, device, create_info);
}

std::expected<Buffer, std::error_code> MemoryAllocator::createBuffer(const vk::BufferCreateInfo &buffer_info, const MemoryAllocationInfo &alloc_info) const noexcept
{
    auto expected_memory = m_allocator.allocateBuffer(buffer_info, alloc_info);
    if (not expected_memory) { return std::unexpected {expected_memory.error()}; }
    vk::DeviceAddress device_address = 0;
    if ((buffer_info.usage & vk::BufferUsageFlagBits::eShaderDeviceAddress) and m_bda_enabled) {
        device_address = m_device.getBufferAddress({expected_memory->handle()});
    }
    return Buffer {std::move(*expected_memory), device_address};
}

std::expected<Image, std::error_code> MemoryAllocator::createImage(const vk::ImageCreateInfo &image_info, const MemoryAllocationInfo &alloc_info) const noexcept
{
    auto expected_memory = m_allocator.allocateImage(image_info, alloc_info);
    if (not expected_memory) { return std::unexpected {expected_memory.error()}; }
    return Image {std::move(*expected_memory)};
}

}