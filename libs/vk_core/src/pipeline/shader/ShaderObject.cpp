#include "vk_core/pipeline/shader/ShaderObject.h"
#include "vk_core/pipeline/shader/entry.h"
#include "vk_core/pipeline/shader/info_structs.h"
#include "vk_core/manifest/DeviceExtensionManifest.h"
#include <array>
#include <algorithm>
#include <ranges>

namespace stdr = std::ranges;
namespace stdv = std::views;

namespace {

using namespace lcf::vkc;

struct BatchCreateInfos
{
    std::vector<vk::ShaderCreateInfoEXT> create_infos;
    std::vector<vk::ShaderStageFlagBits> stages;
};

//- nextStage is a property of the set, not of a single stage: only a whole program can
//- say which stage follows which. stages are visited in pipeline order, which is the
//- ascending order of the vk::ShaderStageFlagBits values themselves
BatchCreateInfos build_batch_create_infos(
    const ShaderProgramInfo & program_info,
    std::span<const vk::DescriptorSetLayout> set_layouts,
    vk::ShaderCreateFlagsEXT flags) noexcept;

} // anonymous namespace

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

namespace lcf::vkc {


std::error_code ShaderObject::create(vk::Device device, const vk::ShaderCreateInfoEXT & info) noexcept
{
    try {
        auto result_value = device.createShaderEXTUnique(info);
        if (result_value.result != vk::Result::eSuccess) { return vk::make_error_code(result_value.result); }
        m_shader_rh = std::move(result_value.value);
    } catch (const vk::SystemError & e) {
        return e.code();
    }
    m_stage = info.stage;
    return {};
}

std::expected<ShaderObject::List, std::error_code> ShaderObject::createBatch(
    vk::Device device, const ShaderProgramInfo & program_info) noexcept
{
    auto set_layouts = program_info.viewDescriptorSetLayouts() | std::ranges::to<std::vector>();
    auto batch = build_batch_create_infos(program_info, set_layouts, {});
    if (batch.create_infos.empty()) { return List {}; }
    try {
        auto result_value = device.createShadersEXTUnique(batch.create_infos);
        if (result_value.result != vk::Result::eSuccess) { return std::unexpected(vk::make_error_code(result_value.result)); }
        List objects;
        objects.reserve(result_value.value.size());
        for (auto && [shader, stage] : std::views::zip(result_value.value, batch.stages)) {
            objects.emplace_back(ShaderObject(std::move(shader), stage));
        }
        return objects;
    } catch (const vk::SystemError & e) {
        return std::unexpected(e.code());
    }
}

std::error_code LinkedShaderObjectGroup::create(vk::Device device, const ShaderProgramInfo & program_info) noexcept
{
    auto set_layouts = program_info.viewDescriptorSetLayouts() | std::ranges::to<std::vector>();
    auto batch = build_batch_create_infos(program_info, set_layouts, vk::ShaderCreateFlagBitsEXT::eLinkStage);
    if (batch.create_infos.empty()) { return {}; }
    try {
        auto result_value = device.createShadersEXTUnique(batch.create_infos);
        if (result_value.result != vk::Result::eSuccess) { return result_value.result; }
        m_objects.reserve(result_value.value.size());
        for (auto && [shader, stage] : stdv::zip(result_value.value, batch.stages)) {
            m_objects.emplace_back(std::move(shader), stage);
        }
    } catch (const vk::SystemError & e) {
        return e.code();
    }
    return {};
}

std::error_code ShaderObjectGroup::create(
    vk::Device device,
    const ShaderProgramInfo & program_info,
    vk::ShaderCreateFlagsEXT flags = {}) noexcept
{
    auto stage_infos = program_info.viewStageInfos() |
        stdv::transform(&ShaderStageInfo::getStage) |
        stdr::to<std::vector>();
    return {};
}

} // namespace lcf::vkc

namespace {

BatchCreateInfos build_batch_create_infos(
    const ShaderProgramInfo & program_info,
    std::span<const vk::DescriptorSetLayout> set_layouts,
    vk::ShaderCreateFlagsEXT flags) noexcept
{
    auto stage_info_ps = program_info.viewStageInfos()
        | std::views::transform([](const auto & stage_info) { return &stage_info; })
        | std::ranges::to<std::vector>();
    std::ranges::sort(stage_info_ps, {}, [](const auto * stage_info_p) {
        return static_cast<uint32_t>(stage_info_p->getStage());
    });

    BatchCreateInfos batch;
    batch.create_infos.reserve(stage_info_ps.size());
    batch.stages.reserve(stage_info_ps.size());
    for (std::size_t i = 0; i < stage_info_ps.size(); ++i) {
        const auto & stage_info = *stage_info_ps[i];
        vk::ShaderStageFlags next_stage = (i + 1 < stage_info_ps.size())
            ? vk::ShaderStageFlags {stage_info_ps[i + 1]->getStage()}
            : vk::ShaderStageFlags {};
        batch.create_infos.emplace_back()
            .setFlags(flags)
            .setStage(stage_info.getStage())
            .setNextStage(next_stage)
            .setCodeType(vk::ShaderCodeTypeEXT::eSpirv)
            .setCode<uint32_t>(stage_info.getCode())
            .setPName(stage_info.getEntryPoint().c_str())
            .setSetLayouts(set_layouts)
            .setPushConstantRanges(stage_info.getPushConstantRanges())
            .setPSpecializationInfo(&stage_info.getSpecializationInfo());
        batch.stages.emplace_back(stage_info.getStage());
    }
    return batch;
}

} // anonymous namespace
