#include "shader_core/ShaderCompiler.h"
#include "shader_core/ShaderIncluder.h"
#include "shader_core/shader_utils.h"
#include <shaderc/shaderc.hpp>
#include <slang.h>
#include <slang-com-ptr.h>
#include <ranges>
#include "file_utils.h"
#include "log.h"

using namespace lcf;
namespace stdr = std::ranges;
namespace stdv = std::views;

void ShaderCompiler::addMacroDefinition(std::string_view macro_definition)
{
    m_macro_definitions.emplace_back(macro_definition);
}

void ShaderCompiler::addIncludeDirectory(const std::filesystem::path &include_directory)
{
    m_include_directories.emplace_back(include_directory.string());
}

std::expected<spirv::Unit, std::error_code> ShaderCompiler::compileGlslSourceToSpv(
    ShaderTypeFlagBits type,
    const std::string & source_code,
    const std::string & shader_name,
    const std::string & entry_point,
    bool optimize) noexcept
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
        lcf_log_error("shader compilation failed: {}", result.GetErrorMessage());
        return std::unexpected(std::make_error_code(std::errc::invalid_argument));
    }
    return spirv::Unit {type, {result.cbegin(), result.cend()}, entry_point};
}

std::expected<spirv::Unit, std::error_code> ShaderCompiler::compileGlslSourceToSpv(ShaderTypeFlagBits type, const std::filesystem::path &file_path) noexcept
{
    auto expected_file_content = read_file_as_string(file_path);
    if (not expected_file_content) {
        lcf_log_error("failed to read file {}: {}", file_path.string(), expected_file_content.error().message());
        return std::unexpected(expected_file_content.error());
    }
    return this->compileGlslSourceToSpv(
        type,
        expected_file_content.value(),
        file_path.filename().string()); 
}

std::expected<spirv::UnitList, std::error_code> ShaderCompiler::compileSlangSourceToSpv(const std::string & source_code, const std::string & module_name) noexcept
{
    using Slang::ComPtr;
    static ComPtr<slang::IGlobalSession> s_global_session_cp;
    static SlangResult s_global_session_initialized = [](ComPtr<slang::IGlobalSession> & session_cp) {
        return slang::createGlobalSession(session_cp.writeRef());
    }(s_global_session_cp);

    slang::SessionDesc session_desc{};
    slang::TargetDesc target_desc{};
    target_desc.format = SLANG_SPIRV;
    target_desc.profile = s_global_session_cp->findProfile("spirv_1_5");
    session_desc.targets = &target_desc;
    session_desc.targetCount = 1;

    std::vector<slang::CompilerOptionEntry> compiler_options;
    session_desc.compilerOptionEntries = compiler_options.data();
    session_desc.compilerOptionEntryCount = compiler_options.size();

    auto search_paths = m_include_directories | stdv::transform([](const auto & dir) { return dir.c_str(); }) | stdr::to<std::vector>();
    session_desc.searchPaths = search_paths.data();
    session_desc.searchPathCount = search_paths.size();

    ComPtr<slang::ISession> session_cp;
    s_global_session_cp->createSession(session_desc, session_cp.writeRef());

    ComPtr<slang::IBlob> diag_cp;
    auto slang_module = session_cp->loadModuleFromSourceString(
        module_name.c_str(),
        module_name.c_str(),
        source_code.c_str(),
        diag_cp.writeRef()
    );
    if (not slang_module) {
        lcf_log_error("slang module load failed: {}", static_cast<const char *>(diag_cp->getBufferPointer()));
        return std::unexpected(std::make_error_code(std::errc::invalid_argument));
    }

    int32_t ep_count = slang_module->getDefinedEntryPointCount();
    std::vector<ComPtr<slang::IEntryPoint>> entry_points;
    entry_points.reserve(ep_count);
    std::vector<slang::IComponentType *> components;
    components.reserve(ep_count + 1);
    components.emplace_back(slang_module);
    for (int i = 0; i < ep_count; ++i) {
        ComPtr<slang::IEntryPoint> ep;
        slang_module->getDefinedEntryPoint(i, ep.writeRef());
        components.emplace_back(ep.get());
        entry_points.emplace_back(std::move(ep));
    }

    ComPtr<slang::IComponentType> composed_cp;
    session_cp->createCompositeComponentType(components.data(), components.size(), composed_cp.writeRef(), diag_cp.writeRef());

    ComPtr<slang::IComponentType> linked_cp;
    composed_cp->link(linked_cp.writeRef(), diag_cp.writeRef());

    spirv::UnitList unit_list;
    for (int i = 0; i < ep_count; ++i) {
        ComPtr<slang::IBlob> spv_blob;
        linked_cp->getEntryPointCode(i, 0, spv_blob.writeRef(), diag_cp.writeRef());
        std::span<const uint32_t> data_span(static_cast<const uint32_t *>(spv_blob->getBufferPointer()), spv_blob->getBufferSize() / sizeof(uint32_t));
        auto * ep_reflection = linked_cp->getLayout()->getEntryPointByIndex(i);
        ShaderTypeFlagBits stage = enum_cast<ShaderTypeFlagBits>(ep_reflection->getStage());
        unit_list.emplace_back(stage, data_span | stdr::to<std::vector>(), ep_reflection->getName());
    }
    return unit_list;
}

std::expected<spirv::UnitList, std::error_code> ShaderCompiler::compileSlangSourceToSpv(const std::filesystem::path & file_path) noexcept
{
    auto expected_file_content = read_file_as_string(file_path);
    if (not expected_file_content) {
        lcf_log_error("failed to read file {}: {}", file_path.string(), expected_file_content.error().message());
        return std::unexpected(expected_file_content.error());
    }
    return this->compileSlangSourceToSpv(expected_file_content.value(), file_path.stem().string());
}
