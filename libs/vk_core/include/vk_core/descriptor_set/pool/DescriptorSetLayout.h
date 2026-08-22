#pragma once

#include <vulkan/vulkan.hpp>

namespace lcf::vkc {

class DescriptorSetLayoutInfo;

}

namespace lcf::vkc::dsp {

class DescriptorSetLayout
{
    using Self = DescriptorSetLayout;
public:
    ~DescriptorSetLayout() noexcept = default;
    DescriptorSetLayout() noexcept = default;
    DescriptorSetLayout(const Self &) noexcept = default;
    DescriptorSetLayout(Self &&) noexcept = default;
    Self & operator=(const Self &) noexcept = default;
    Self & operator=(Self &&) noexcept = default;
public:
    std::error_code create(vk::Device device, const DescriptorSetLayoutInfo & info) noexcept;
private:
};


} // namespace lcf::vkc::dsp