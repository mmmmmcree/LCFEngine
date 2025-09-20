#pragma once

#include "ShaderDefs.h"
#include "ShaderResource.h"
#include "PointerDefs.h"

namespace lcf {
    class Shader : public STDPointerDefs<Shader>
    {
    public:
        Shader(ShaderTypeFlagBits type) : m_stage(type), m_entry_point("main") { }
        virtual ~Shader() = default;
		operator bool() const { return this->isCompiled(); }
        virtual bool compileGlslFile(std::string_view file_path) { return false; };
		virtual bool isCompiled() const { return false; }
		void setEntryPoint(std::string_view entry_point) { m_entry_point = entry_point; }
        ShaderTypeFlagBits getStage() const { return m_stage; }
		const ShaderResources & getResources() const { return m_resources; }
    protected:
        ShaderTypeFlagBits m_stage;
		std::string m_entry_point;
		ShaderResources m_resources;
    };
}