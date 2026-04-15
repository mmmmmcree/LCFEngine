#include "Vulkan/VulkanShaderProgram.h"
#include "Vulkan/VulkanContext.h"
#include "Vulkan/vulkan_enums.h"
#include "log.h"
#include <format>

using namespace lcf::render;
namespace stdr = std::ranges;
namespace stdv = std::views;

lcf::render::VulkanShaderProgram::VulkanShaderProgram(VulkanContext *context) :
    m_context_p(context)
{
}

lcf::render::VulkanShaderProgram::~VulkanShaderProgram()
{
}

VulkanShaderProgram & lcf::render::VulkanShaderProgram::addShaderFromGlslFile(ShaderTypeFlagBits stage, std::string_view file_path)
{
    VulkanShader::SharedPointer shader = VulkanShader::makeShared(m_context_p, stage);
    auto error_code = shader->compileGlslFile(file_path);
    if (error_code) { return *this; }
    m_stage_to_shader_map[shader->getStage()] = shader;
    return *this;
}

std::error_code lcf::render::VulkanShaderProgram::link()
{
    if (this->isLinked()) { return std::make_error_code(std::errc::invalid_argument); }
    this->createShaderStageInfoList();
    this->createDescriptorSetLayoutBindingTable();
    this->createDescriptorSetLayouts();
    this->createPipelineLayout();
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

void lcf::render::VulkanShaderProgram::createShaderStageInfoList()
{
    for (const auto &[stage, shader] : m_stage_to_shader_map) {
        m_shader_stage_info_list.emplace_back(shader->getShaderStageInfo());
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
        vk::ShaderStageFlagBits vk_stage = enum_cast<vk::ShaderStageFlagBits>(stage);
        const ShaderResources &resources = shader->getResources();
        for (const auto &resource : resources.uniform_buffers) {
            resource_info_list.emplace_back(vk_stage, vk::DescriptorType::eUniformBuffer, resource);
        }
        for (const auto &resource : resources.sampled_images) {
            resource_info_list.emplace_back(vk_stage, vk::DescriptorType::eCombinedImageSampler, resource);
        }
        for (const auto &resource : resources.separate_images) {
            resource_info_list.emplace_back(vk_stage, vk::DescriptorType::eSampledImage, resource);
        }
        for (const auto &resource : resources.separate_samplers) {
            resource_info_list.emplace_back(vk_stage, vk::DescriptorType::eSampler, resource);
        }
        for (const auto &resource : resources.storage_images) {
            resource_info_list.emplace_back(vk_stage, vk::DescriptorType::eStorageImage, resource);
        }
        for (const auto &resource : resources.storage_buffers) {
            resource_info_list.emplace_back(vk_stage, vk::DescriptorType::eStorageBuffer, resource);
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

static std::unordered_map<uint32_t, vkenums::DescriptorSetStrategy> read_strategy(const lcf::JSON & pragmas);

void VulkanShaderProgram::createDescriptorSetLayouts()
{
    std::unordered_map<uint32_t, vkenums::DescriptorSetStrategy> strategy_map;
    for (const auto &[stage, shader] : m_stage_to_shader_map) {
        strategy_map.insert_range(read_strategy(shader->getPragmas()));
    }
    for (uint32_t set = 0; set < m_descriptor_set_layout_binding_table.size(); ++set) {
        auto bindings = m_descriptor_set_layout_binding_table[set]; // copy — may be modified
        auto layout_sp = VulkanDescriptorSetLayout::makeShared();
        if (strategy_map.contains(set)) {
            auto strategy = strategy_map.at(set);
            lcf_log_info("set has strategy");
        }
        uint32_t bindless_buffer_index = vkenums::decode::get_index(vkenums::DescriptorSetIndex::eBindlessBuffers);
        if (set == bindless_buffer_index and not bindings.empty()) {
            layout_sp->setFlags(vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool);
        }

        layout_sp->setBindings(bindings)
            .setIndex(set)
            .create(m_context_p);
        m_descriptor_set_layout_sp_list.emplace_back(std::move(layout_sp));
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
    for (const auto & layout_sp : m_descriptor_set_layout_sp_list) {
        descriptor_set_layout_list.emplace_back(layout_sp->getHandle());
    }
    pipeline_layout_info.setSetLayouts(descriptor_set_layout_list)
        .setPushConstantRanges(push_constant_range_list);
    try {
        m_pipeline_layout = device.createPipelineLayoutUnique(pipeline_layout_info);
    } catch (const vk::SystemError &e) {
        std::runtime_error error(std::format("failed to create pipeline layout: {}", e.what()));
        lcf_log_error(error.what());
        throw error;
    }
}

static std::unordered_map<uint32_t, vkenums::DescriptorSetStrategy> read_strategy(const lcf::JSON & pragmas)
{
    constexpr std::string_view pragma_name = "descriptor_set_strategy";
    constexpr std::string_view set_key = "set";
    constexpr std::string_view strategy_key = "strategy";
    constexpr std::string_view bindless_strategy = "bindless";
    if (not pragmas.contains(pragma_name)) { return {}; }
    const auto & pragma_list = pragmas.at(pragma_name);
    std::unordered_map<uint32_t, vkenums::DescriptorSetStrategy> strategy_map;
    for (const auto &pragma : pragma_list) {
        if (not pragma.contains(set_key) or not pragma.contains(strategy_key)) { continue; }
        uint32_t set = std::stoul(pragma.at(set_key).get<std::string>());
        const auto &strategy = pragma.at(strategy_key).get<std::string>();
        if (strategy == bindless_strategy) {
            strategy_map[set] = vkenums::DescriptorSetStrategy::eBindless;
        }
    }
    return strategy_map;
}