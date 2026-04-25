#include "shader_core/ShaderCompiler.h"
#include "shader_core/ShaderIncluder.h"
#include "log.h"
#include <shaderc/shaderc.hpp>

using namespace lcf;

void lcf::ShaderCompiler::addMacroDefinition(std::string_view macro_definition)
{
    m_macro_definitions.emplace_back(macro_definition);
}

void lcf::ShaderCompiler::addIncludeDirectory(std::string_view include_directory)
{
    m_include_directories.emplace_back(include_directory);
}

lcf::spirv::Code lcf::ShaderCompiler::compileGlslSourceToSpv(
    ShaderTypeFlagBits type,
    const std::string & source_code,
    const std::string & shader_name,
    const std::string & entry_point,
    bool optimize)
{
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    for (const auto &macro_definition : m_macro_definitions) {
        options.AddMacroDefinition(macro_definition.c_str());
    }
    if (not m_include_directories.empty()) {
        auto includer = std::make_unique<ShaderIncluder>();
        for (const auto &include_directory : m_include_directories) {
            includer->addIncludeDirectory(include_directory.c_str());
        }
        options.SetIncluder(std::move(includer));
    }
    if (optimize) { options.SetOptimizationLevel(shaderc_optimization_level_size); }
    shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(source_code, enum_cast<shaderc_shader_kind>(type), shader_name.c_str(), entry_point.c_str(), options);
    if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
        std::runtime_error error(std::format("shader compilation failed: {}", result.GetErrorMessage()));
        lcf_log_error(error.what());
        throw error;
    }
    return {result.cbegin(), result.cend()};
}
