#include "vk_core/pipeline/shader/ShaderObject.h"
#include "vk_core/pipeline/shader/entry.h"
#include "vk_core/pipeline/shader/info_structs.h"
#include "vk_core/manifest/DeviceExtensionManifest.h"
#include "vk_core/command/CommandBufferProxy.h"
#include <array>
#include <vector>
#include <ranges>

namespace stdr = std::ranges;
namespace stdv = std::views;

namespace lcf::vkc::entry {

void register_shader_object(DeviceExtensionManifest & manifest) noexcept
{
    static constexpr std::array k_extensions { vk::EXTShaderObjectExtensionName };
    static constexpr std::array k_features
    {
        LCF_VKC_UTILS_FEATURE_BIT(&vk::PhysicalDeviceShaderObjectFeaturesEXT::shaderObject),
    };
    manifest.addRequiredExtensions(k_extensions)
        .addRequiredFeatures(k_features);
}

} // namespace lcf::vkc::entry

namespace {

template <typename BitType>
constexpr void for_each_flag_bit(vk::Flags<BitType> flags, auto && visitor) noexcept
{
    using MaskType = typename vk::Flags<BitType>::MaskType;
    auto bits = static_cast<MaskType>(flags);
    while (bits) {
        auto lowest = bits & ~(bits - 1);
        visitor(static_cast<BitType>(lowest));
        bits ^= lowest;
    }
}

} // anonymous namespace

namespace lcf::vkc {

std::error_code ShaderObject::create(vk::Device device, const vk::ShaderCreateInfoEXT & info) noexcept
{
    try {
        auto [result, shader] = device.createShaderEXTUnique(info);
        if (result != vk::Result::eSuccess) { return vk::make_error_code(result); }
        m_shader_rh = std::move(shader);
    } catch (const vk::SystemError & e) {
        return e.code();
    }
    m_stage = info.stage;
    m_next_stages = info.nextStage;
    return {};
}

std::error_code ShaderObjectGroup::create(
    vk::Device device,
    const ShaderProgramInfo & program_info,
    vk::ShaderCreateFlagsEXT flags) noexcept
{
    auto set_layouts = program_info.viewDescriptorSetLayouts();
    auto stage_infos = program_info.viewStageInfos();
    if (stage_infos.empty()) { return {}; }
    std::vector<vk::ShaderCreateInfoEXT> shader_infos(stage_infos.size());
    vk::ShaderStageFlags next_stage {};
    for (auto && [shader_info, stage_info] : stdv::zip(shader_infos, stage_infos) | stdv::reverse) {
        if (not enum_traits<vk::ShaderStageFlagBits>::is_valid_next_stage_of(stage_info.getStage(), next_stage)) {
            return std::make_error_code(std::errc::invalid_argument);
        }
        shader_info.setFlags(flags)
            .setStage(stage_info.getStage())
            .setNextStage(std::exchange(next_stage, stage_info.getStage()))
            .setCodeType(vk::ShaderCodeTypeEXT::eSpirv)
            .setCode<uint32_t>(stage_info.getCode())
            .setPName(stage_info.getEntryPoint().c_str())
            .setSetLayouts(set_layouts)
            .setPushConstantRanges(stage_info.getPushConstantRanges())
            .setPSpecializationInfo(&stage_info.getSpecializationInfo());
    }
    try {
        auto [result, shaders] = device.createShadersEXTUnique(shader_infos);
        if (result != vk::Result::eSuccess) { return vk::make_error_code(result); }
        m_objects = stdv::zip_transform([](auto & shader, const auto & shader_info) {
            return std::make_pair(shader_info.stage, ShaderObject {std::move(shader), shader_info.stage, shader_info.nextStage}); },
            shaders, shader_infos) | stdr::to<ShaderObjectMap>();
    } catch (const vk::SystemError & e) {
        return e.code();
    }
    return {};
}

auto ShaderObjectBindingState::clear() noexcept -> Self &
{
    m_handles.clear();
    m_leases.clear();
    return *this;
}

auto ShaderObjectBindingState::setStage(const ShaderObject & shader) noexcept -> Self &
{
    const auto stage = shader.getStage();
    m_handles.insert_or_assign(stage, shader.handle());
    m_leases.insert_or_assign(stage, shader.lease());
    return *this;
}

auto ShaderObjectBindingState::unsetStages(vk::ShaderStageFlags stages) noexcept -> Self &
{
    for_each_flag_bit(stages, [this](vk::ShaderStageFlagBits stage) {
        this->m_handles.insert_or_assign(stage, vk::ShaderEXT {});
        this->m_leases.insert_or_assign(stage, ResourceLease {});
    });
    return *this;
}

auto ShaderObjectBindingState::removeStages(vk::ShaderStageFlags stages) noexcept -> Self &
{
    for_each_flag_bit(stages, [this](vk::ShaderStageFlagBits stage) {
        this->m_handles.erase(stage);
        this->m_leases.erase(stage);
    });
    return *this;
}

auto ShaderObjectBindingState::assign(const ShaderObjectGroup & group) noexcept -> Self &
{
    this->clear();
    return this->merge(group);
}

auto ShaderObjectBindingState::merge(const ShaderObjectGroup & group) noexcept -> Self &
{
    for (const auto & shader : group.viewObjects()) { this->setStage(shader); }
    return *this;
}

void ShaderObjectBindingState::bind(CommandBufferProxy & cmd) const noexcept
{
    if (m_handles.empty()) { return; }
    cmd.bindShadersEXT(m_handles.keys(), m_handles.values());
    cmd.pinLeases(m_leases.values());
}

} // namespace lcf::vkc
