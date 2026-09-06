#pragma once

#include <vulkan/vulkan.hpp>

namespace lcf::vkc {

class DescriptorSetBindingInfo
{
    using Self = DescriptorSetBindingInfo;
public:
    ~DescriptorSetBindingInfo() noexcept = default;
    constexpr DescriptorSetBindingInfo(
        vk::DescriptorType descriptor_type = {},
        uint32_t descriptor_count = 0u,
        vk::ShaderStageFlags stage_flags = {},
        vk::DescriptorBindingFlags flags = {}) :
        m_descriptor_type(descriptor_type),
        m_descriptor_count(descriptor_count),
        m_stage_flags(stage_flags),
        m_binding_flags(flags) {}
    DescriptorSetBindingInfo(const Self &) = default;
    DescriptorSetBindingInfo(Self &&) noexcept = default;
    Self & operator=(const Self &) = default;
    Self & operator=(Self &&) noexcept = default;
public:
    Self & addFlags(vk::DescriptorBindingFlags flags) noexcept { m_binding_flags |= flags; return *this; }
    Self & setDescriptorType(vk::DescriptorType type) noexcept { m_descriptor_type = type; return *this; }
    Self & setDescriptorCount(uint32_t count) noexcept { m_descriptor_count = count; return *this; }
    Self & addStageFlags(vk::ShaderStageFlags flags) noexcept { m_stage_flags |= flags; return *this; }
    Self & addBindingFlags(vk::DescriptorBindingFlags flags) noexcept { m_binding_flags |= flags; return *this; }
    const vk::DescriptorType & getDescriptorType() const noexcept { return m_descriptor_type; }
    const uint32_t & getDescriptorCount() const noexcept { return m_descriptor_count; }
    const vk::ShaderStageFlags & getStageFlags() const noexcept { return m_stage_flags; }
    const vk::DescriptorBindingFlags & getBindingFlags() const noexcept { return m_binding_flags; }
private:
    vk::DescriptorType m_descriptor_type;
    uint32_t m_descriptor_count;
    vk::ShaderStageFlags m_stage_flags;
    vk::DescriptorBindingFlags m_binding_flags;
};

class DescriptorSetLayoutInfo
{
    using Self = DescriptorSetLayoutInfo;
    using BindingList = std::vector<vk::DescriptorSetLayoutBinding>;
    using BindingFlagsList = std::vector<vk::DescriptorBindingFlags>;
public:
    Self & addLayoutFlags(vk::DescriptorSetLayoutCreateFlags flags) noexcept { m_layout_flags |= flags; return *this; }
    Self & addBindingInfo(
        vk::DescriptorType descriptor_type,
        uint32_t descriptor_count,
        vk::ShaderStageFlags stage_flags,
        vk::DescriptorBindingFlags flags = {}) noexcept
    {
        uint32_t binding = static_cast<uint32_t>(m_bindings.size());
        m_bindings.emplace_back(binding, descriptor_type, descriptor_count, stage_flags);
        m_flags_list.emplace_back(flags);
        return *this;
    }
    Self & addBindingInfo(const DescriptorSetBindingInfo & binding_info) noexcept
    {
        return this->addBindingInfo(
            binding_info.getDescriptorType(),
            binding_info.getDescriptorCount(),
            binding_info.getStageFlags(),
            binding_info.getBindingFlags());
    };
    const vk::DescriptorSetLayoutCreateFlags & getLayoutFlags() const noexcept { return m_layout_flags; }
    const BindingList & getBindings() const noexcept { return m_bindings; }
    const BindingFlagsList & getBindingFlags() const noexcept { return m_flags_list; }
private:
    vk::DescriptorSetLayoutCreateFlags m_layout_flags {};
    BindingList m_bindings;
    BindingFlagsList m_flags_list;
};


} // namespace lcf::vkc