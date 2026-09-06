#pragma once

#include <vulkan/vulkan.hpp>
#include "resource_utils.h"
#include "vk_core/utils/ResourceHandle.h"

namespace lcf::vkc {

class DescriptorSetLayoutInfo;

}

namespace lcf::vkc::dsb {

class DescriptorSetLayout
{
    using Self = DescriptorSetLayout;
    using LayoutResourceHandle = utils::ResourceHandle<vk::DescriptorSetLayout>;
    using BindingList = std::vector<vk::DescriptorSetLayoutBinding>;
    using LayoutOffsetList = std::vector<vk::DeviceSize>;
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
    ResourceLease lease() const noexcept { return m_layout.lease(); }
    const BindingList & getBindings() const noexcept { return m_bindings; }
    const vk::DescriptorBindingFlags & getBindingFlags() const noexcept { return m_flags; }
    const vk::DeviceSize & getLayoutSize() const noexcept { return m_layout_size; }
    const LayoutOffsetList & getLayoutOffsets() const noexcept { return m_layout_offsets; }
private:
    LayoutResourceHandle m_layout;
    vk::DescriptorBindingFlags m_flags = {};
    BindingList m_bindings;
    vk::DeviceSize m_layout_size = 0u;
    LayoutOffsetList m_layout_offsets;
};

} // namespace lcf::vkc::dsb
