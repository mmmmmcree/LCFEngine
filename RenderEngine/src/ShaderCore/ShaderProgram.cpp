#include "ShaderCore/ShaderProgram.h"

lcf::Shader *lcf::ShaderProgram::getShader(ShaderTypeFlagBits stage) const
{
    if (not m_stage_to_shader_map.contains(stage)) { return nullptr; }
    return m_stage_to_shader_map.at(stage).get();
}

void lcf::ShaderProgram::addShader(const Shader::SharedPointer &shader)
{
    if (m_stage_to_shader_map.contains(shader->getStage()) or not shader->isCompiled()) { return; }
    m_stage_to_shader_map[shader->getStage()] = shader;
}

bool lcf::ShaderProgram::link()
{
    m_is_linked = std::ranges::all_of(m_stage_to_shader_map, [](const auto &pair) { return pair.second->isCompiled(); });
    return m_is_linked;
}