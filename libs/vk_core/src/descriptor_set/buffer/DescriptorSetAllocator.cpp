#include "vk_core/descriptor_set/buffer/DescriptorSetAllocator.h"
#include "vk_core/descriptor_set/buffer/DescriptorSetLayout.h"
#include "vk_core/memory/MemoryAllocator.h"
#include "vk_core/memory/info_structs.h"
#include "vk_core/utils/align.h"
#include "vk_core/error.h"

namespace lcf::vkc::dsb {

DescriptorSetAllocator::~DescriptorSetAllocator() noexcept = default;

DescriptorSetAllocator::DescriptorSetAllocator(Self &&) noexcept = default;

DescriptorSetAllocator & DescriptorSetAllocator::operator=(Self &&) noexcept = default;

std::error_code DescriptorSetAllocator::create(const MemoryAllocator & memory_allocator) noexcept
{
    if (m_memory_allocator_p) { return make_error_code(errc::already_created); }
    m_memory_allocator_p = &memory_allocator;
    vk::PhysicalDevice physical_device = memory_allocator.getPhysicalDevice();
    vk::PhysicalDeviceProperties2 properties;
    properties.pNext = &m_descriptor_buffer_properties;
    physical_device.getProperties2(&properties);
    return {};
}

std::expected<DescriptorSetAllocation, std::error_code> DescriptorSetAllocator::allocate(const DescriptorSetLayout & layout, uint32_t count) noexcept
{
    const auto alignment = static_cast<std::size_t>(m_descriptor_buffer_properties.descriptorBufferOffsetAlignment);
    const vk::DeviceSize per_descriptor_size = utils::align_up(layout.getLayoutSize(), alignment);
    vk::BufferCreateInfo buffer_info;
    buffer_info.setSize(per_descriptor_size * count)
        .setUsage(vk::BufferUsageFlagBits::eTransferDst |
            vk::BufferUsageFlagBits::eShaderDeviceAddress |
            vk::BufferUsageFlagBits::eResourceDescriptorBufferEXT |
            vk::BufferUsageFlagBits::eSamplerDescriptorBufferEXT)
        .setSharingMode(vk::SharingMode::eExclusive);
    MemoryAllocationInfo mem_alloc_info;
    mem_alloc_info.setAccess(MemoryAccess::eDeviceLocal);
    vkc::Buffer ds_buffer;
    if (auto ec = ds_buffer.create(*m_memory_allocator_p, buffer_info, mem_alloc_info)) { return std::unexpected(ec); }
    return DescriptorSetAllocation {std::move(ds_buffer), count, per_descriptor_size};
}

void DescriptorSetAllocator::recycle(const DescriptorSetAllocation &) noexcept
{
}

} // namespace lcf::vkc::dsb
