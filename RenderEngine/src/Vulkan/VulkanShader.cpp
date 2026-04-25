#include "Vulkan/VulkanShader.h"
#include "shader_core/ShaderCompiler.h"
#include "shader_core/shader_utils.h"
#include "Vulkan/vulkan_enums.h"
#include "log.h"
#include "file_utils.h"
#include <ranges>

using namespace lcf;
using namespace lcf::render;
namespace stdr = std::ranges;
namespace stdv = std::views;

static std::unordered_map<uint32_t, VulkanDescriptorSetLayout> from_shader_resources_to_layouts(
    vk::ShaderStageFlagBits shader_type, const ShaderResources & resources);

VulkanShader & VulkanShader::compileGlslFile(
    ShaderTypeFlagBits type,
    const std::filesystem::path & file_path,
    std::string_view entry_point) noexcept
{
    m_stage = type;
    m_entry_point = entry_point;
    ShaderCompiler compiler;
    compiler.addMacroDefinition("VULKAN_SHADER");
    auto include_dir = file_path.parent_path() / "include";
    compiler.addIncludeDirectory(include_dir.string());
    auto expected_file_content = read_file_as_string(file_path);
    if (not expected_file_content) {
        lcf_log_info("Error reading file: {}", expected_file_content.error().message());
        return *this;
    }
    const auto & file_content = expected_file_content.value();
    m_spv_code = compiler.compileGlslSourceToSpv(
        m_stage,
        file_content,
        file_path.filename().string(),
        m_entry_point, false);
    return *this;
}

std::error_code VulkanShader::create(vk::Device device) noexcept
{
    vk::ShaderModuleCreateInfo module_info;
    module_info.setCode(m_spv_code);
    try {
        m_module = device.createShaderModuleUnique(module_info);
    } catch (const vk::SystemError & e) {
        lcf_log_debug(e.what());
        return e.code();
    }
    m_resources = spirv::analyze(m_spv_code);
    m_layout_map = from_shader_resources_to_layouts(enum_cast<vk::ShaderStageFlagBits>(m_stage), m_resources);
    for (auto & [_, layout] : m_layout_map) {
        if (auto ec = layout.create(device, vkenums::DescriptorSetStrategy::eIndividual)) { return ec; }
    }
    std::exchange(m_spv_code, {}); //clear
    return {};
}

vk::PipelineShaderStageCreateInfo VulkanShader::getShaderStageInfo() const noexcept
{
    return vk::PipelineShaderStageCreateInfo(
        {},
        enum_cast<vk::ShaderStageFlagBits>(m_stage),
        m_module.get(),
        m_entry_point.c_str());
}

std::unordered_map<uint32_t, VulkanDescriptorSetLayout> from_shader_resources_to_layouts(
    vk::ShaderStageFlagBits shader_type, const ShaderResources &resources)
{
    using ResourceInfo = std::pair<vk::DescriptorType, const ShaderResource &>;
    std::vector<ResourceInfo> resource_info_list;
    for (const auto &resource : resources.uniform_buffers) {
        resource_info_list.emplace_back(vk::DescriptorType::eUniformBuffer, resource);
    }
    for (const auto &resource : resources.sampled_images) {
        resource_info_list.emplace_back(vk::DescriptorType::eCombinedImageSampler, resource);
    }
    for (const auto &resource : resources.separate_images) {
        resource_info_list.emplace_back(vk::DescriptorType::eSampledImage, resource);
    }
    for (const auto &resource : resources.separate_samplers) {
        resource_info_list.emplace_back(vk::DescriptorType::eSampler, resource);
    }
    for (const auto &resource : resources.storage_images) {
        resource_info_list.emplace_back(vk::DescriptorType::eStorageImage, resource);
    }
    for (const auto &resource : resources.storage_buffers) {
        resource_info_list.emplace_back(vk::DescriptorType::eStorageBuffer, resource);
    }
    std::unordered_map<uint32_t, VulkanDescriptorSetLayout> layout_map;
    for (const auto & resource_info : resource_info_list) {
        const auto & shader_resource = resource_info.second;
        auto & layout = layout_map[shader_resource.getSet()];
        VulkanDescriptorSetBinding binding;
        binding.setDescriptorType(resource_info.first)
           .setDescriptorCount(shader_resource.getArraySize())
           .setStageFlags(shader_type)
           .setBindingIndex(shader_resource.getBinding());
        layout.addBinding(binding);
    }
    return layout_map;
}
