#include "Vulkan/VulkanShader.h"
#include "Vulkan/VulkanContext.h"
#include "ShaderCore/ShaderCompiler.h"
#include "log.h"


lcf::render::VulkanShader::VulkanShader(VulkanContext * context, ShaderTypeFlagBits type) :
    Shader(type),
    m_context_p(context)
{
}

lcf::render::VulkanShader::~VulkanShader()
{
    for (auto & layout : m_descriptor_set_layout_list) {
        m_context_p->getDevice().destroyDescriptorSetLayout(layout);
    }
}

lcf::render::VulkanShader::operator bool() const
{
    return this->isCompiled();
}

bool lcf::render::VulkanShader::compileGlslFile(const std::filesystem::path & file_path)
{
    ShaderCompiler compiler;
    compiler.addMacroDefinition("VULKAN_SHADER");
    auto include_dir = file_path.parent_path() / "include";
    compiler.addIncludeDirectory(include_dir.string());
    auto spirv_code =  compiler.compileGlslSourceFileToSpv(file_path, m_stage, m_entry_point.c_str());
    m_resources = compiler.analyzeSpvCode(spirv_code);
    vk::ShaderModuleCreateInfo module_info;
    module_info.setCode(spirv_code);
    try {
        m_module = m_context_p->getDevice().createShaderModuleUnique(module_info);
    } catch (const vk::Error& e) {
        lcf_log_debug(e.what());
    }
    return this->isCompiled();
}

bool lcf::render::VulkanShader::isCompiled() const
{
    return m_module.get();
}

vk::PipelineShaderStageCreateInfo lcf::render::VulkanShader::getShaderStageInfo() const
{
    return vk::PipelineShaderStageCreateInfo({}, enum_cast<vk::ShaderStageFlagBits>(m_stage), m_module.get(), m_entry_point.c_str());
}
