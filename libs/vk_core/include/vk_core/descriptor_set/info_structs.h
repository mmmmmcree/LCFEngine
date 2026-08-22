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
        m_flags(flags) {}
    DescriptorSetBindingInfo(const Self &) = default;
    DescriptorSetBindingInfo(Self &&) noexcept = default;
    Self & operator=(const Self &) = default;
    Self & operator=(Self &&) noexcept = default;
public:
    Self & addFlags(vk::DescriptorBindingFlags flags) noexcept { m_flags |= flags; return *this; }
    Self & setDescriptorType(vk::DescriptorType type) noexcept { m_descriptor_type = type; return *this; }
    Self & setDescriptorCount(uint32_t count) noexcept { m_descriptor_count = count; return *this; }
    Self & addStageFlags(vk::ShaderStageFlags flags) noexcept { m_flags |= flags; return *this; }
    bool containsFlags(vk::DescriptorBindingFlagBits flags) const noexcept { return static_cast<bool>(m_flags & flags); }
    const uint32_t & getBindingIndex() const noexcept { return m_binding.binding; }
    const vk::DescriptorType & getDescriptorType() const noexcept { return m_binding.descriptorType; }
    const uint32_t & getDescriptorCount() const noexcept { return m_binding.descriptorCount; }
    const vk::ShaderStageFlags & getStageFlags() const noexcept { return m_binding.stageFlags; }
    const vk::DescriptorBindingFlags & getFlags() const noexcept { return m_flags; }
    const vk::DescriptorSetLayoutBinding & getLayoutBinding() const noexcept { return m_binding; }
private:
    vk::DescriptorType m_descriptor_type;
    uint32_t m_descriptor_count;
    vk::ShaderStageFlags m_stage_flags;
    vk::DescriptorBindingFlags m_flags;
};

class DescriptorSetLayoutInfo
{
    using Self = DescriptorSetLayoutInfo;
    using BindingInfoList = std::vector<vk::DescriptorSetLayoutBinding>;
public:
    Self & addBindingInfo(
        vk::DescriptorType descriptor_type,
        uint32_t descriptor_count,
        vk::ShaderStageFlags stage_flags,
        vk::DescriptorBindingFlags flags = {}) noexcept
    {
        uint32_t binding = static_cast<uint32_t>(m_bindings.size());
        m_bindings.emplace_back(binding, descriptor_type, descriptor_count, stage_flags);
        m_flags |= flags;
        return *this;
    }
    Self & addBindingInfo(const DescriptorSetBindingInfo & binding_info) noexcept
    {
        return this->addBindingInfo(
            binding_info.getDescriptorType(),
            binding_info.getDescriptorCount(),
            binding_info.getStageFlags(),
            binding_info.getFlags());
    };
private:
    BindingInfoList m_bindings;
    vk::DescriptorBindingFlags m_flags = {};
};


} // namespace lcf::vkc