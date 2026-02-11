#pragma once

#include <spirv_cross/spirv_cross.hpp>
#include "ShaderResource.h"
#include <filesystem>
#include <string>
#include <vector>

namespace lcf {
    using SpvCode = std::vector<uint32_t>;

    class ShaderCompiler
    {
    public:
        ShaderCompiler() = default;
        ~ShaderCompiler() noexcept = default;
        ShaderCompiler(const ShaderCompiler &) = delete;
        ShaderCompiler & operator=(const ShaderCompiler &) = delete;
        ShaderCompiler(ShaderCompiler &&) = default;
        ShaderCompiler & operator=(ShaderCompiler &&) = default;
    public:
        void addMacroDefinition(std::string_view macro_definition);
        void addIncludeDirectory(std::string_view include_directory);
        SpvCode compileGlslSourceToSpv(const char *shader_name, ShaderTypeFlagBits type, const char *source_code, const char * entry_point = "main", bool optimize = false);
        SpvCode compileGlslSourceFileToSpv(const std::filesystem::path & file_path, ShaderTypeFlagBits type, const char * entry_point = "main", bool optimize = false);
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