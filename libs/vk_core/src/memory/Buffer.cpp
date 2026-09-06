#include "vk_core/memory/Buffer.h"
#include "vk_core/memory/MemoryAllocator.h"
#include "vk_core/memory/info_structs.h"
#include "vk_core/error.h"
#include <vulkan/vulkan.hpp>
#include <ranges>

namespace stdr = std::ranges;

namespace lcf::vkc {

std::error_code Buffer::create(
    const MemoryAllocator & allocator,
    const vk::BufferCreateInfo & buffer_info,
    const MemoryAllocationInfo & alloc_info) noexcept
{
    if (m_memory_rh) { return make_error_code(errc::already_created); }
    auto expected_memory = allocator.allocateBuffer(buffer_info, alloc_info);
    if (not expected_memory) { return expected_memory.error(); }
    m_memory_rh = std::move(expected_memory.value());
    vk::Device device = allocator.getDevice();
    if (allocator.isBufferDeviceAddressEnabled() and buffer_info.usage & vk::BufferUsageFlagBits::eShaderDeviceAddress) {
        vk::BufferDeviceAddressInfo bda_info;
        bda_info.setBuffer(m_memory_rh->handle());
        m_device_address = device.getBufferAddress(bda_info);
    }
    m_size = buffer_info.size;
    return {};
}

std::span<std::byte> Buffer::getMappedMemorySpan() const noexcept
{
    return m_memory_rh->getMappedMemorySpan().subspan(0, m_size);
}

std::error_code Buffer::copyFromMemory(std::span<const std::byte> src, vk::DeviceSize offset_in_bytes) noexcept
{
    return m_memory_rh->copyFromMemory(src, offset_in_bytes);
}

std::error_code Buffer::flush(vk::DeviceSize offset, vk::DeviceSize size) const noexcept
{
    return m_memory_rh->flush(offset, size);
}

} // namespace lcf::vkc
