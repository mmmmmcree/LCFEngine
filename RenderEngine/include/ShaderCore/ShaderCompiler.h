#pragma once

#include "ShaderDefs.h"
#include "ShaderResource.h"
#include <string>
#include <vector>

namespace lcf {
    class ShaderCompiler
    {
    public:
        void addMacroDefinition(std::string_view macro_definition);
        void addIncludeDirectory(std::string_view include_directory);
        SpvCode compileGlslSourceToSpv(const char *shader_name, ShaderTypeFlagBits type, const char *source_code, const char * entry_point = "main", bool optimize = false);
        SpvCode compileGlslSourceFileToSpv(const char *file_path, ShaderTypeFlagBits type, const char * entry_point = "main", bool optimize = false);
        ShaderResources analyzeSpvCode(const SpvCode &spv_code);
    private:
        ShaderResource parseBufferResource(const spirv_cross::Compiler &spv_compiler, const spirv_cross::Resource &resource);
        ShaderResource parseResource(const spirv_cross::Compiler &spv_compiler, const spirv_cross::Resource &resource);
        void parseResourceMembers(ShaderResourceMember &resource, const spirv_cross::Compiler &spv_compiler, spirv_cross::TypeID type_id);
        ShaderResource parseInputResource(const spirv_cross::Compiler &spv_compiler, const spirv_cross::Resource &resource);
        void parseInputResourceMembers(ShaderResourceMember &resource, const spirv_cross::Compiler &spv_compiler, spirv_cross::TypeID type_id);
    private:
        std::vector<std::string> m_macro_definitions;
        std::vector<std::string> m_include_directories;
    };
}