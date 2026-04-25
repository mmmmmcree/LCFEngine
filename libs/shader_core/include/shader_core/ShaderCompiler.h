#pragma once

#include "shader_utils.h"
#include "shader_core_enums.h"
#include "JSON.h"
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace lcf {
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
        lcf::JSON extractPragmas(std::string_view source_code);
        spirv::Code compileGlslSourceToSpv(
            ShaderTypeFlagBits type,
            const std::string & source_code,
            const std::string & shader_name,
            const std::string & entry_point = "main",
            bool optimize = false);
    private:
        std::vector<std::string> m_macro_definitions;
        std::vector<std::string> m_include_directories;
    };
}
