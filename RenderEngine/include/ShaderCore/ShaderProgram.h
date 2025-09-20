#pragma once

#include "Shader.h"
#include "PointerDefs.h"
#include <unordered_map>
#include "ShaderProgramResource.h"

namespace lcf {
    class ShaderProgram : public STDPointerDefs<ShaderProgram>
    {
    public:
        using StageToShaderMap = std::unordered_map<ShaderTypeFlagBits, Shader::SharedPointer>;
        ShaderProgram() = default;
        ~ShaderProgram() = default;
        virtual bool link();
        bool isLinked() const { return m_is_linked; }
        bool containsStage(ShaderTypeFlagBits stage) const { return m_stage_to_shader_map.contains(stage); }
        Shader * getShader(ShaderTypeFlagBits stage) const;
    protected:
        void addShader(const Shader::SharedPointer & shader);
    protected:
        bool m_is_linked = false;
        StageToShaderMap m_stage_to_shader_map;
    };
}