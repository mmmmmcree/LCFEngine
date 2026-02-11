#pragma once

#include "ShaderResource.h"

namespace lcf {
    class ShaderProgramResource 
    {
    public:
        ShaderProgramResource(ShaderTypeFlagBits stage, ShaderResourceType type, const ShaderResource &resource) :
            m_stage(stage), m_type(type), m_resource(resource) {}
        ShaderProgramResource(const ShaderProgramResource &other) :
            m_stage(other.m_stage), m_type(other.m_type), m_resource(other.m_resource) {}
        ShaderProgramResource & operator=(const ShaderProgramResource &other) = delete;
        const ShaderResource & getShaderResource() const { return m_resource; }
        ShaderResourceType getShaderResourceType() const { return m_type; }
        ShaderTypeFlagBits getShaderStage() const { return m_stage; }
    private:
        ShaderResourceType m_type;
        const ShaderResource & m_resource;
        ShaderTypeFlagBits m_stage;
    };
}