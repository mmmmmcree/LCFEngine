#pragma once

#include "ShaderDefs.h"
#include "ShaderResource.h"

namespace lcf {
    class ShaderProgramResource 
    {
    public:
        ShaderProgramResource(ShaderTypeFlagBits stage, ShaderResourceType type, const ShaderResource &resource) :
            m_stage(stage), m_type(type), m_resource(resource)
        {}
        const ShaderResource & getShaderResource() const { return m_resource; }
        ShaderResourceType getShaderResourceType() const { return m_type; }
        ShaderTypeFlagBits getShaderStage() const { return m_stage; }
    private:
        ShaderTypeFlagBits m_stage;
        ShaderResourceType m_type;
        const ShaderResource &m_resource;
    };
}