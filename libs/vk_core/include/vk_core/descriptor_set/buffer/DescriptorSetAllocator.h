#pragma once

#include <vulkan/vulkan.hpp>
#include "resource_utils.h"
#include "vk_core/memory/Buffer.h"
#include <expected>

namespace lcf::vkc {

class MemoryAllocator;
class DescriptorSetLayoutInfo;

}

namespace lcf::vkc::dsb {

class DescriptorSetLayout;
class DescriptorSetProxy;

struct DescriptorSetAllocation
{
    vkc::Buffer m_buffer;
    uint32_t m_count;
    vk::DeviceSize m_per_descriptor_size;
};

class DescriptorSetAllocator
{
    friend class DescriptorSetProxy;
    using Self = DescriptorSetAllocator;
public:
    ~DescriptorSetAllocator() noexcept;
    DescriptorSetAllocator() noexcept = default;
    DescriptorSetAllocator(const Self &) noexcept = delete;
    DescriptorSetAllocator(Self &&) noexcept;
    Self & operator=(const Self &) noexcept = delete;
    Self & operator=(Self &&) noexcept;
public:
    std::error_code create(const MemoryAllocator & memory_allocator) noexcept;
    std::expected<DescriptorSetAllocation, std::error_code> allocate(const DescriptorSetLayout & layout, uint32_t count = 1u) noexcept;
    void recycle(const DescriptorSetAllocation & allocation) noexcept;
// private:
    // void deallocate(DescriptorSetAllocation allocation) noexcept;
private:
    const MemoryAllocator & getMemoryAllocator() const noexcept { return *m_memory_allocator_p; }
    const vk::PhysicalDeviceDescriptorBufferPropertiesEXT & getDescriptorBufferProperties() const noexcept { return m_descriptor_buffer_properties; }
private:
    const MemoryAllocator * m_memory_allocator_p = nullptr;
    vk::PhysicalDeviceDescriptorBufferPropertiesEXT m_descriptor_buffer_properties;
};

} // namespace lcf::vkc::dsb