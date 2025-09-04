#include "VulkanShader.h"
#include "VulkanContext.h"
#include "ShaderCompiler.h"
#include <QFile>
#include <QFileInfo>
#include <QDebug>


lcf::render::VulkanShader::VulkanShader(VulkanContext * context, ShaderTypeFlagBits type) :
    Shader(type),
    m_context(context)
{
}

lcf::render::VulkanShader::~VulkanShader()
{
    for (auto & layout : m_descriptor_set_layout_list) {
        m_context->getDevice().destroyDescriptorSetLayout(layout);
    }
}

lcf::render::VulkanShader::operator bool() const
{
    return this->isCompiled();
}

bool lcf::render::VulkanShader::compileGlslFile(std::string_view file_path)
{
    ShaderCompiler compiler;
    compiler.addMacroDefinition("VULKAN_SHADER");
    QString include_dir = QFileInfo(file_path.data()).path() + "/include";
    compiler.addIncludeDirectory(include_dir.toLocal8Bit().constData());
    auto spirv_code =  compiler.compileGlslSourceFileToSpv(file_path.data(), m_stage, m_entry_point.c_str());
    m_resources = compiler.analyzeSpvCode(spirv_code);
    vk::ShaderModuleCreateInfo module_info;
    module_info.setCode(spirv_code);
    try {
        m_module = m_context->getDevice().createShaderModuleUnique(module_info);
    } catch (const vk::Error& e) {
        qDebug() << "VulkanShader::compileGlslFile: " << e.what();
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
