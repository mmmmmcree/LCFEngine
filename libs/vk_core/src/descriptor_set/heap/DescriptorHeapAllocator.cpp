#include "vk_core/descriptor_set/heap/DescriptorHeapAllocator.h"
#include "vk_core/memory/info_structs.h"
#include "vk_core/memory/MemoryAllocator.h"
#include "vk_core/utils/align.h"
#include "vk_core/error.h"

namespace lcf::vkc::dsh {

std::error_code DescriptorHeapAllocator::create(const MemoryAllocator &memory_allocator) noexcept
{
    if (m_memory_allocator_p) { return make_error_code(errc::already_created); }
    m_memory_allocator_p = &memory_allocator;
    vk::PhysicalDevice physical_device = memory_allocator.getPhysicalDevice();
    vk::PhysicalDeviceProperties2 properties;
    properties.pNext = &m_dsh_props;
    physical_device.getProperties2(&properties);
    return {};
}

std::expected<DescriptorHeapAllocation, std::error_code> DescriptorHeapAllocator::allocate(const DescriptorHeapAllocateInfo &info) noexcept
{
    vk::DeviceSize resource_heap_size = vkc::utils::align_up(info.m_resource_heap_size + m_dsh_props.minResourceHeapReservedRange,  m_dsh_props.resourceHeapAlignment);
    vk::DeviceSize sampler_heap_size = vkc::utils::align_up(info.m_sampler_heap_size + m_dsh_props.minSamplerHeapReservedRange, m_dsh_props.samplerHeapAlignment);
    vk::BufferCreateInfo buffer_info;
    buffer_info.setSize(resource_heap_size)
        .setUsage(vk::BufferUsageFlagBits::eTransferDst |
            vk::BufferUsageFlagBits::eShaderDeviceAddress |
            vk::BufferUsageFlagBits::eDescriptorHeapEXT)
        .setSharingMode(vk::SharingMode::eExclusive);
    MemoryAllocationInfo mem_alloc_info;
    mem_alloc_info.setAccess(MemoryAccess::eDeviceLocal);
    vkc::Buffer resource_heap_buffer, sampler_heap_buffer;
    if (auto ec = resource_heap_buffer.create(this->getMemoryAllocator(), buffer_info, mem_alloc_info)) { return std::unexpected(ec); }
    buffer_info.setSize(sampler_heap_size);
    if (auto ec = sampler_heap_buffer.create(this->getMemoryAllocator(), buffer_info, mem_alloc_info)) { return std::unexpected(ec); }
    return DescriptorHeapAllocation {
        std::move(resource_heap_buffer),
        std::move(sampler_heap_buffer),
        resource_heap_size,
        sampler_heap_size,
    };
}

} // namespace lcf::vkc::dsh


