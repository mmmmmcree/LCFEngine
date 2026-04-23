#include "Vulkan/VulkanShaderProgram.h"
#include "Vulkan/vulkan_enums.h"
#include "log.h"
#include <format>

using namespace lcf::render;
namespace stdr = std::ranges;
namespace stdv = std::views;

lcf::render::VulkanShaderProgram::~VulkanShaderProgram()
{
}

VulkanShaderProgram & lcf::render::VulkanShaderProgram::addShaderFromGlslFile(ShaderTypeFlagBits stage, std::string_view file_path)
{
    auto shader = std::make_shared<VulkanShader>();
    shader->compileGlslFile(stage, file_path);
    m_stage_to_shader_map[shader->getStage()] = shader;
    return *this;
}

VulkanShaderProgram & lcf::render::VulkanShaderProgram::specifyDescriptorSetLayout(ResourceRef<const VulkanDescriptorSetLayout> layout)
{
    m_descriptor_set_layout_ref_map.emplace(std::make_pair(layout->getIndex(), layout));
    return *this;
}

std::error_code lcf::render::VulkanShaderProgram::link(vk::Device device) noexcept
{
    if (this->isLinked()) { return std::make_error_code(std::errc::invalid_argument); }
    for (const auto &[stage, shader] : m_stage_to_shader_map) {
        if (auto ec = shader->create(device)) { return ec; }
        m_shader_stage_info_list.emplace_back(shader->getShaderStageInfo());
    }
    if (auto ec = this->createDescriptorSetLayouts(device)) { return ec; }
    if (auto ec = this->createPipelineLayout(device)) { return ec; }
    return {};
}

bool lcf::render::VulkanShaderProgram::hasVertexInput() const noexcept
{
    if (not this->isLinked()) { return false; }
    bool has_vertex_stage = m_stage_to_shader_map.contains(ShaderTypeFlagBits::eVertex);
    if (not has_vertex_stage) { return false; }
    const auto &vertex_shader = m_stage_to_shader_map.at(ShaderTypeFlagBits::eVertex);
    return not vertex_shader->getResources().stage_inputs.empty();
}

void lcf::render::VulkanShaderProgram::setPushConstantData(vk::ShaderStageFlags stage, std::span<const void *> data_list)
{
    auto it = m_push_constant_map.find(static_cast<uint32_t>(stage));
    if (it == m_push_constant_map.end()) { return; }
    it->second.setData(data_list);
}

void lcf::render::VulkanShaderProgram::setPushConstantData(vk::ShaderStageFlags stage, const std::initializer_list<const void *> &data_list)
{
    auto it = m_push_constant_map.find(static_cast<uint32_t>(stage));
    if (it == m_push_constant_map.end()) { return; }
    it->second.setData(data_list);
}

void lcf::render::VulkanShaderProgram::bindPushConstants(vk::CommandBuffer cmd)
{
    for (const auto &[stage, push_constant] : m_push_constant_map) {
        push_constant.bind(cmd, this->getPipelineLayout());
    }
}

std::error_code VulkanShaderProgram::createDescriptorSetLayouts(vk::Device device) noexcept
{
    for (const auto & [stage, shader] : m_stage_to_shader_map) {
        for (const auto & [set, layout] : shader->getDescriptorLayouts()) {
            if (not m_descriptor_set_layout_ref_map.contains(set)) {
                m_descriptor_set_layout_ref_map.emplace(std::make_pair(set, ResourceRef<const VulkanDescriptorSetLayout>(layout)));
                continue;
            }
            const auto & bindings = layout.getBindings();
            const auto & exist_bindings = m_descriptor_set_layout_ref_map.at(set)->getBindings();
            if (bindings != exist_bindings) { continue; }
            auto & program_layout = m_descriptor_set_layout_map[set];
            VulkanDescriptorSetLayoutBindings new_bindings = exist_bindings;    
            for (auto & binding : new_bindings) {
                binding.addStageFlags(enum_cast<vk::ShaderStageFlagBits>(stage));
            }
            program_layout.setBindings(new_bindings);
        }
    }
    for (auto &[set, layout] : m_descriptor_set_layout_map) {
        if (auto ec = layout.create(device, vkenums::DescriptorSetStrategy::eIndividual)) { return ec; }
        m_descriptor_set_layout_ref_map.emplace(std::make_pair(set, ResourceRef<const VulkanDescriptorSetLayout>(layout)));
    }
    return {};
}

std::error_code lcf::render::VulkanShaderProgram::createPipelineLayout(vk::Device device) noexcept
{
    //! push constants with same name are regarded as the same range
    /**
     * @brief create push constant ranges
     * step1: use push_constant_range_map to merge push constant ranges with the same name;
     * step2: the offset of each push constant range is set to the sum of the previous push constant ranges' size;
     * step3: build m_push_constant_map according to the push_constant_range_map, one push constant for each stage;
     * step4: transform m_push_constant_map to push_constant_range_list;
     */
    std::unordered_map<std::string_view, vk::PushConstantRange> push_constant_range_map;
    for (const auto &[stage, shader] : m_stage_to_shader_map) {
        const ShaderResources &resources = shader->getResources();
        vk::ShaderStageFlags stage_flag_bits = enum_cast<vk::ShaderStageFlagBits>(stage);
        for (const auto &resource : resources.push_constant_buffers) {
            auto &push_constant_range = push_constant_range_map[resource.getName()];
            push_constant_range.setOffset(0u)
               .setSize(resource.getSizeInBytes())
               .setStageFlags(push_constant_range.stageFlags | stage_flag_bits);
        }
    } // step1
    uint32_t offset = 0u;
    for (auto &[_, push_constant_range] : push_constant_range_map) {
        push_constant_range.setOffset(offset);
        offset += push_constant_range.size;
    } // step2
    for (const auto &[stage, shader] : m_stage_to_shader_map) {
        const ShaderResources &resources = shader->getResources();
        uint32_t stage_flag_bits = static_cast<uint32_t>(enum_cast<vk::ShaderStageFlagBits>(stage));
        for (const auto &resource : resources.push_constant_buffers) {
            auto &push_constant_range = push_constant_range_map[resource.getName()];
            VulkanPushConstant &push_constant = m_push_constant_map[stage_flag_bits];
            push_constant.setStageFlags(push_constant_range.stageFlags)
                .setOffset(push_constant_range.offset);
            const auto &members = resource.getMembers();
            for (uint32_t i = 0; i < members.size(); ++i) {
                const auto &member = members[i];
                push_constant.setDataLayout(i, member.getOffset(), member.getSizeInBytes());
            }
        }
    } // step3
    std::vector<vk::PushConstantRange> push_constant_range_list;
    push_constant_range_list.reserve(push_constant_range_map.size());
    for (auto &[_, push_constant_range] : push_constant_range_map) {
        push_constant_range_list.emplace_back(std::move(push_constant_range));
    } // step4

    vk::PipelineLayoutCreateInfo pipeline_layout_info;
    std::vector<vk::DescriptorSetLayout> descriptor_set_layout_list;
    for (const auto & [_, layout_ref] : m_descriptor_set_layout_ref_map) {
        descriptor_set_layout_list.emplace_back(layout_ref->getHandle());
    }
    pipeline_layout_info.setSetLayouts(descriptor_set_layout_list)
        .setPushConstantRanges(push_constant_range_list);
    try {
        m_pipeline_layout = device.createPipelineLayoutUnique(pipeline_layout_info);
    } catch (const vk::SystemError &e) {
        lcf_log_error(e.what());
        return e.code();
    }
    return {};
}