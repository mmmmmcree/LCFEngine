#pragma once

#include "shader_core_enums.h"
#include "spirv.h"
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
#include <expected>

namespace lcf {
    class ShaderCompiler
    {
    public:
        ~ShaderCompiler() noexcept = default;
        ShaderCompiler();
        ShaderCompiler(const ShaderCompiler &) = delete;
        ShaderCompiler & operator=(const ShaderCompiler &) = delete;
        ShaderCompiler(ShaderCompiler &&) = default;
        ShaderCompiler & operator=(ShaderCompiler &&) = default;
    public:
        void addMacroDefinition(std::string_view macro_definition);
        void addIncludeDirectory(const std::filesystem::path & include_directory);
        std::expected<spirv::Unit, std::error_code> compileGlslSourceToSpv(
            ShaderTypeFlagBits type,
            const std::string & source_code,
            const std::string & shader_name,
            bool optimize = false) noexcept;
        std::expected<spirv::Unit, std::error_code> compileGlslSourceToSpv(ShaderTypeFlagBits type, const std::filesystem::path & file_path) noexcept;
        std::expected<spirv::UnitList, std::error_code> compileSlangSourceToSpv(const std::string & source_code, const std::string & module_name) noexcept;
        std::expected<spirv::UnitList, std::error_code> compileSlangSourceToSpv(const std::filesystem::path & file_path) noexcept;
    private:
        std::vector<std::string> m_macro_definitions;
        std::vector<std::string> m_include_directories;
    };
}
