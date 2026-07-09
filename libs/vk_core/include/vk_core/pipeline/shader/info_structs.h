#pragma once

#include <vulkan/vulkan.hpp>
#include <vector>
#include <string>
#include <string_view>
#include <span>
#include <unordered_map>
#include <map>
#include <optional>
#include <concepts>
#include "concepts/range_concept.h"
#include "bytes.h"

namespace lcf::vkc {

class ShaderStageInfo
{
    using Self = ShaderStageInfo;
    using Code = std::vector<uint32_t>;
    using CodeView = std::span<const uint32_t>;
    using PushConstantRangeList = std::vector<vk::PushConstantRange>;
    using SpecializationMapEntryList = std::vector<vk::SpecializationMapEntry>;
    using Bytes = std::vector<std::byte>;
public:
    ~ShaderStageInfo() noexcept = default;
    ShaderStageInfo() noexcept = default;
    ShaderStageInfo(const Self & other) = default;
    ShaderStageInfo(Self && other) noexcept = default;
    Self & operator=(const Self & other) = default;
    Self & operator=(Self && other) noexcept = default;
public:
    Self & setStage(vk::ShaderStageFlagBits stage) noexcept { m_stage = stage; return *this; }
    Self & setCode(Code code) noexcept { m_code = std::move(code); return *this; }
    Self & setCode(CodeView code) noexcept { m_code.assign_range(code); return *this; }
    Self & setEntryPoint(std::string entry_point) noexcept { m_entry_point = std::move(entry_point); return *this; }
    Self & setEntryPoint(std::string_view entry_point) noexcept { m_entry_point = entry_point; return *this; }
    Self & addPushConstantRange(uint32_t offset, uint32_t size) noexcept
    {
        m_push_constant_ranges.emplace_back(vk::PushConstantRange{{}, offset, size});
        return *this;
    }
    template <typename T>
    requires (std::integral<T> or std::floating_point<T>) and (not std::same_as<T, bool>)
    Self & addSpecializationMapEntry(uint32_t constant_id, T value) noexcept
    {
        m_specialization_map_entries.emplace_back(constant_id, static_cast<uint32_t>(m_specialization_data.size()), sizeof(T));
        m_specialization_data.append_range(as_bytes_from_value(value));
        m_specialization_info.setMapEntries(m_specialization_map_entries)
            .setData<std::byte>(m_specialization_data);
        return *this;
    }
    const vk::ShaderStageFlagBits & getStage() const noexcept { return m_stage; }
    const Code & getCode() const noexcept { return m_code; }
    const std::string & getEntryPoint() const noexcept { return m_entry_point; }
    const PushConstantRangeList & getPushConstantRanges() const noexcept { return m_push_constant_ranges; }
    const vk::SpecializationInfo & getSpecializationInfo() const noexcept { return m_specialization_info; }
private:
    vk::ShaderStageFlagBits m_stage;
    Code m_code;
    std::string m_entry_point;
    PushConstantRangeList m_push_constant_ranges;
    Bytes m_specialization_data;
    SpecializationMapEntryList m_specialization_map_entries;
    vk::SpecializationInfo m_specialization_info;
};

class ShaderProgramInfo
{
    using Self = ShaderProgramInfo;
    using StageInfoMap = std::unordered_map<vk::ShaderStageFlagBits, ShaderStageInfo>;
    using DescriptorSetLayoutMap = std::map<uint32_t, vk::DescriptorSetLayout>;    
public:
    ~ShaderProgramInfo() noexcept = default;
    ShaderProgramInfo() noexcept = default;
    ShaderProgramInfo(const Self & other) = default;
    ShaderProgramInfo(Self && other) noexcept = default;
    Self & operator=(const Self & other) = default;
    Self & operator=(Self && other) noexcept = default;
public:
    Self & addStageInfo(ShaderStageInfo stage_info) noexcept
    {
        m_stage_info_map.emplace(stage_info.getStage(), std::move(stage_info));
        return *this;
    }
    Self & addDescriptorSetLayout(uint32_t set, vk::DescriptorSetLayout descriptor_set_layout) noexcept
    {
        m_descriptor_set_layout_map.emplace(set, descriptor_set_layout);
        return *this;
    }
    auto viewStageInfos() const noexcept { return m_stage_info_map | std::views::values; }
    auto viewDescriptorSetLayouts() const noexcept { return m_descriptor_set_layout_map | std::views::values; } // view by order
private:
    StageInfoMap m_stage_info_map;
    DescriptorSetLayoutMap m_descriptor_set_layout_map;
};

} // namespace lcf::vkc