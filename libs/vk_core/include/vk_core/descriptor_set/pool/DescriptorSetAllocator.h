#pragma once

#include <vulkan/vulkan.hpp>
#include "DescriptorSetProxy.h"
#include <expected>

namespace lcf::vkc {

class DescriptorSetLayoutInfo;

}

namespace lcf::vkc::dsp {

class DescriptorSetLayout;

class DescriptorSetAllocatorInfo
{

};

class DescriptorSetAllocator
{
    using Self = DescriptorSetAllocator;
public:
    ~DescriptorSetAllocator() noexcept = default;
    DescriptorSetAllocator() noexcept = default;
    DescriptorSetAllocator(const Self &) noexcept = default;
    DescriptorSetAllocator(Self &&) noexcept = default;
    Self & operator=(const Self &) noexcept = default;
    Self & operator=(Self &&) noexcept = default;
public:
    std::error_code create(vk::Device device, const DescriptorSetAllocatorInfo & info) noexcept;
    std::expected<DescriptorSetProxy, std::error_code> allocate(const DescriptorSetLayout & layout) noexcept;
private:
};


} // namespace lcf::vkc::dsp