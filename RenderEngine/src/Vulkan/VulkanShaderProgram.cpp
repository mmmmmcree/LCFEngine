#include "VulkanShaderProgram.h"
#include "VulkanContext.h"
#include "error.h"
#include <format>

using namespace lcf::render;

lcf::render::VulkanShaderProgram::VulkanShaderProgram(VulkanContext *context) :
    ShaderProgram(),
    m_context_p(context)
{
}

lcf::render::VulkanShaderProgram::~VulkanShaderProgram()
{
}

VulkanShaderProgram & lcf::render::VulkanShaderProgram::addShaderFromGlslFile(ShaderTypeFlagBits stage, std::string_view file_path)
{
    VulkanShader::SharedPointer shader = VulkanShader::makeShared(m_context_p, stage);
    if (shader->compileGlslFile(file_path)) {
        this->addShader(shader);
    }
    return *this;
}

bool lcf::render::VulkanShaderProgram::link()
{
    ShaderProgram::link();
    if (not this->isLinked()) { return false; }
    this->createShaderStageInfoList();
    this->createDescriptorSetLayoutBindingTable();
    this->createDescriptorSetPrototypes();
    this->createPipelineLayout();
    return m_is_linked;
}

bool lcf::render::VulkanShaderProgram::hasVertexInput() const noexcept
{
    if (not m_is_linked) { return false; }
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

void lcf::render::VulkanShaderProgram::createShaderStageInfoList()
{
    for (const auto &[stage, shader] : m_stage_to_shader_map) {
        auto vulkan_shader = std::static_pointer_cast<VulkanShader>(shader);
        m_shader_stage_info_list.emplace_back(vulkan_shader->getShaderStageInfo());
    }
}

void lcf::render::VulkanShaderProgram::createDescriptorSetLayoutBindingTable()
{
    struct ResourceInfo
    {
        ResourceInfo(vk::ShaderStageFlagBits s, vk::DescriptorType t, const ShaderResource &r) :
            stage(s), descriptor_type(t), shader_resource(r) {}
        vk::ShaderStageFlagBits stage;
        vk::DescriptorType descriptor_type;
        const ShaderResource &shader_resource;
    };
    std::vector<ResourceInfo> resource_info_list;
    for (const auto &[stage, shader] : m_stage_to_shader_map) {
        const ShaderResources &resources = shader->getResources();
        for (const auto &resource : resources.uniform_buffers) {
            resource_info_list.emplace_back(enum_cast<vk::ShaderStageFlagBits>(stage), vk::DescriptorType::eUniformBufferDynamic, resource);
        }
        for (const auto &resource : resources.sampled_images) {
            resource_info_list.emplace_back(enum_cast<vk::ShaderStageFlagBits>(stage), vk::DescriptorType::eCombinedImageSampler, resource);
        }
        for (const auto &resource : resources.separate_images) {
            resource_info_list.emplace_back(enum_cast<vk::ShaderStageFlagBits>(stage), vk::DescriptorType::eSampledImage, resource);
        }
        for (const auto &resource : resources.separate_samplers) {
            resource_info_list.emplace_back(enum_cast<vk::ShaderStageFlagBits>(stage), vk::DescriptorType::eSampler, resource);
        }
        for (const auto &resource : resources.storage_images) {
            resource_info_list.emplace_back(enum_cast<vk::ShaderStageFlagBits>(stage), vk::DescriptorType::eStorageImage, resource);
        }
        for (const auto &resource : resources.storage_buffers) {
            resource_info_list.emplace_back(enum_cast<vk::ShaderStageFlagBits>(stage), vk::DescriptorType::eStorageBuffer, resource);
        }
        // todo add other resource types
    }
    if (resource_info_list.empty()) { return ; }
    std::map<uint32_t, uint32_t> set_to_max_binding_map;
    for (const auto &resource_info : resource_info_list) {
        const auto &shader_resource = resource_info.shader_resource;
        uint32_t &max_binding = set_to_max_binding_map[shader_resource.getSet()];
        max_binding = std::max(max_binding, shader_resource.getBinding());
    }
    uint32_t max_set = set_to_max_binding_map.rbegin()->first;
    m_descriptor_set_layout_binding_table.resize(max_set + 1);
    for (uint32_t set = 0; set <= max_set; ++set) {
        if (not set_to_max_binding_map.contains(set)) { continue; }
        m_descriptor_set_layout_binding_table[set].resize(set_to_max_binding_map[set] + 1);
    }
    for (const auto &resource_info : resource_info_list) {
        const auto &shader_resource = resource_info.shader_resource;
        auto &descriptor_set_layout_binding = m_descriptor_set_layout_binding_table[shader_resource.getSet()][shader_resource.getBinding()];
        descriptor_set_layout_binding.setDescriptorType(resource_info.descriptor_type)
           .setDescriptorCount(shader_resource.getArraySize())
           .setStageFlags(descriptor_set_layout_binding.stageFlags | resource_info.stage)
           .setBinding(shader_resource.getBinding());
    }

}

void lcf::render::VulkanShaderProgram::createDescriptorSetPrototypes()
{
    for (uint32_t set = 0; set < m_descriptor_set_layout_binding_table.size(); ++set) {
        const auto &descriptor_set_layout_bindings = m_descriptor_set_layout_binding_table[set];
        auto & ds_prototype = m_descriptor_set_prototype_list.emplace_back(descriptor_set_layout_bindings, set);
        ds_prototype.create(m_context_p);
    }
}

void lcf::render::VulkanShaderProgram::createPipelineLayout()
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

    auto device = m_context_p->getDevice();
    vk::PipelineLayoutCreateInfo pipeline_layout_info;
    std::vector<vk::DescriptorSetLayout> descriptor_set_layout_list;
    for (const auto & ds_prototype : m_descriptor_set_prototype_list) {
        descriptor_set_layout_list.emplace_back(ds_prototype.getLayout());
    }
    pipeline_layout_info.setSetLayouts(descriptor_set_layout_list)
        .setPushConstantRanges(push_constant_range_list);
    try {
        m_pipeline_layout = device.createPipelineLayoutUnique(pipeline_layout_info);
    } catch (const vk::SystemError &e) {
        LCF_THROW_RUNTIME_ERROR(std::format("lcf::render::VulkanShaderProgram::createPipelineLayout(): failed to create pipeline layout: {}", e.what()));
    }
}
