#include "vk_core/memory/Buffer.h"
#include "vk_core/memory/MemoryAllocator.h"
#include "vk_core/memory/info_structs.h"
#include "vk_core/error.h"
#include <vulkan/vulkan.hpp>
#include <ranges>

namespace stdr = std::ranges;

namespace lcf::vkc {

std::error_code BufferView::create(const Buffer & buffer, vk::DeviceSize offset, vk::DeviceSize range) noexcept
{
    if (m_memory_rh) { return make_error_code(errc::already_created); }
    if (not buffer.m_memory_rh or range == 0u) { return std::make_error_code(std::errc::invalid_argument); }
    if (offset >= buffer.m_size or range + offset > buffer.m_size) { return std::make_error_code(std::errc::result_out_of_range); }
    if (range == vk::WholeSize) { range = buffer.m_size - offset; }
    m_memory_rh = buffer.m_memory_rh;
    m_offset = offset;
    m_range = range;
    m_device_address = buffer.m_device_address ? buffer.m_device_address + offset : 0u;
    return {};
}

std::span<std::byte> BufferView::getMappedMemorySpan() const noexcept
{
    return m_memory_rh->getMappedMemorySpan().subspan(m_offset, m_range);
}

std::error_code BufferView::copyFromMemory(std::span<const std::byte> src, vk::DeviceSize offset_in_bytes) noexcept
{
    return m_memory_rh->copyFromMemory(src, m_offset + offset_in_bytes);
}

std::error_code BufferView::flush(vk::DeviceSize offset, vk::DeviceSize size) const noexcept
{
    if (size == vk::WholeSize) { size = m_range - offset; }
    return m_memory_rh->flush(m_offset + offset, size);
}

std::expected<BufferView, std::error_code> Buffer::createView(vk::DeviceSize offset, vk::DeviceSize range) const noexcept
{
    BufferView view;
    if (auto ec = view.create(*this, offset, range)) { return std::unexpected(ec); }
    return view;
}

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
