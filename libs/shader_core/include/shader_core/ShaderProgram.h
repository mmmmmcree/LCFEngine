#pragma once

#include "shader_core_fwd_decls.h"
#include "shader_core_enums.h"
#include <unordered_map>

namespace lcf {
    class ShaderProgram : public ShaderProgramPointerDefs
    {
    public:
        using StageToShaderMap = std::unordered_map<ShaderTypeFlagBits, ShaderSharedPointer>;
        ShaderProgram() = default;
        virtual ~ShaderProgram() = default;
        virtual bool link();
        bool isLinked() const { return m_is_linked; }
        bool containsStage(ShaderTypeFlagBits stage) const { return m_stage_to_shader_map.contains(stage); }
        Shader * getShader(ShaderTypeFlagBits stage) const;
    protected:
        void addShader(const ShaderSharedPointer & shader);
    protected:
        bool m_is_linked = false;
        StageToShaderMap m_stage_to_shader_map;
    };
}