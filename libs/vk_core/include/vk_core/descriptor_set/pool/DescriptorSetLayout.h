#pragma once

#include <vulkan/vulkan.hpp>
#include "resource_utils.h"
#include "vk_core/utils/ResourceHandle.h"

namespace lcf::vkc {

class DescriptorSetLayoutInfo;

}

namespace lcf::vkc::dsp {

class DescriptorSetLayout
{
    using Self = DescriptorSetLayout;
    using LayoutResourceHandle = utils::ResourceHandle<vk::DescriptorSetLayout>;
    using BindingList = std::vector<vk::DescriptorSetLayoutBinding>;
public:
    ~DescriptorSetLayout() noexcept = default;
    DescriptorSetLayout() noexcept = default;
    DescriptorSetLayout(const Self &) noexcept = default;
    DescriptorSetLayout(Self &&) noexcept = default;
    Self & operator=(const Self &) noexcept = default;
    Self & operator=(Self &&) noexcept = default;
    operator const vk::DescriptorSetLayout &() const noexcept { return this->handle(); }
public:
    std::error_code create(vk::Device device, const DescriptorSetLayoutInfo & info) noexcept;
    const vk::DescriptorSetLayout & handle() const noexcept { return m_layout.get(); }
    const BindingList & getBindings() const noexcept { return m_bindings; }
    const vk::DescriptorBindingFlags & getBindingFlags() const noexcept { return m_flags; }
private:
    LayoutResourceHandle m_layout;
    vk::DescriptorBindingFlags m_flags = {};
    BindingList m_bindings;
};


} // namespace lcf::vkc::dsp