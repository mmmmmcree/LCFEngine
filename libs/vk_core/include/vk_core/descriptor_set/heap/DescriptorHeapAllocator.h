#pragma once

#include <vulkan/vulkan.hpp>
#include "resource_utils.h"
#include "vk_core/memory/Buffer.h"
#include <expected>

namespace lcf::vkc::dsh {

class DescriptorHeapProxy;

struct DescriptorHeapAllocateInfo
{
    vk::DeviceSize m_resource_heap_size = 0u;
    vk::DeviceSize m_sampler_heap_size = 0u;
};

struct DescriptorHeapAllocation
{
    vkc::Buffer m_resource_buffer;
    vkc::Buffer m_sampler_buffer;
    // vk::BindHeapInfoEXT m_resource_bind_info;
    // vk::BindHeapInfoEXT m_sampler_bind_info;
    vk::DeviceSize m_resource_size = 0u;
    vk::DeviceSize m_sampler_size = 0u;
};

class DescriptorHeapAllocator
{
    friend class DescriptorHeapProxy;
    using Self = DescriptorHeapAllocator;
public:
    ~DescriptorHeapAllocator() noexcept = default;
    DescriptorHeapAllocator() noexcept = default;
    DescriptorHeapAllocator(const Self &) noexcept = delete;
    DescriptorHeapAllocator(Self &&) noexcept;
    Self & operator=(const Self &) noexcept = delete;
    Self & operator=(Self &&) noexcept;
public:
    std::error_code create(const MemoryAllocator & memory_allocator) noexcept;
    std::expected<DescriptorHeapAllocation, std::error_code> allocate(const DescriptorHeapAllocateInfo & info) noexcept;
private:
    const MemoryAllocator & getMemoryAllocator() const noexcept { return *m_memory_allocator_p; }
    const vk::PhysicalDeviceDescriptorHeapPropertiesEXT & getDescriptorHeapProperties() const noexcept { return m_dsh_props; }
private:
    const MemoryAllocator * m_memory_allocator_p = nullptr;
    vk::PhysicalDeviceDescriptorHeapPropertiesEXT m_dsh_props;
};

} // namespace lcf::vkc::dsh
