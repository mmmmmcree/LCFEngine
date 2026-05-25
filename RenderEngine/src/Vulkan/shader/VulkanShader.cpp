#include "Vulkan/shader/VulkanShader.h"
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

std::error_code VulkanShader::create(vk::Device device) noexcept
{
    vk::ShaderModuleCreateInfo module_info;
    module_info.setCode(m_spv_unit.getCode());
    try {
        m_module = device.createShaderModuleUnique(module_info);
    } catch (const vk::SystemError & e) {
        lcf_log_debug(e.what());
        return e.code();
    }
    m_resources = spirv::analyze(m_spv_unit.getCode());
    m_layout_map = from_shader_resources_to_layouts(enum_cast<vk::ShaderStageFlagBits>(this->getStage()), m_resources);
    for (auto & [_, layout] : m_layout_map) {
        if (auto ec = layout.create(device, vkenums::DescriptorSetStrategy::eIndividual)) { return ec; }
    }
    std::exchange(m_spv_unit.m_code, {}); //clear
    return {};
}

vk::PipelineShaderStageCreateInfo VulkanShader::getShaderStageInfo() const noexcept
{
    return vk::PipelineShaderStageCreateInfo(
        {},
        enum_cast<vk::ShaderStageFlagBits>(this->getStage()),
        m_module.get(),
        m_spv_unit.getEntryPoint().c_str());
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
