#include "shader_core/ShaderCompiler.h"
#include "shader_core/shader_utils.h"
#include "shader_core/config.h"
#include "shader_core/ShaderCache.h"
#include "shader_core/Manifest.h"
#include "shader_core/hash.h"
#include <shaderc/shaderc.hpp>
#include <slang.h>
#include <slang-com-ptr.h>
#include <ranges>
#include "file_utils.h"
#include "log.h"
#include "enums/enum_name.h"
#include "bytes.h"

using namespace lcf;
using namespace lcf::sc;
namespace stdr = std::ranges;
namespace stdv = std::views;

class GlslShaderIncluder : public shaderc::CompileOptions::IncluderInterface
{
public:
    GlslShaderIncluder(const std::vector<std::string> & search_paths) : m_search_paths(search_paths) {}
public:
    virtual shaderc_include_result * GetInclude(
        const char* requested_source,
        shaderc_include_type type,
        const char* requesting_source,
        size_t include_depth) override
    {
        if (type == shaderc_include_type_relative) {
            auto requesting_dir = std::filesystem::path(requested_source).parent_path();
            auto full_path = requesting_dir / requested_source;
            if (auto result = this->createIncludeResult(full_path)) { return result; }
        }
        for (const auto &path : m_search_paths) {
            auto full_path = std::filesystem::path(path) / requested_source;
            if (auto result = this->createIncludeResult(full_path)) { return result; }
        }
        return nullptr;
    }
    virtual void ReleaseInclude(shaderc_include_result* include_result) override
    {
        delete[] include_result->source_name;
        delete[] include_result->content;
        delete include_result;
    }
private:
    shaderc_include_result * createIncludeResult(const std::filesystem::path & path)
    {
        auto expected_file_content = read_file_as_string(path);
        if (not expected_file_content) { return nullptr; }
        auto path_str = path.string();
        char * source_name = new char[path_str.size() + 1]; source_name[path_str.size()] = '\0';
        stdr::copy(path_str, source_name);
        const auto & file_content = expected_file_content.value();
        char * content = new char[file_content.size() + 1]; content[file_content.size()] = '\0';
        stdr::copy(file_content, content);
        shaderc_include_result * include_result = new shaderc_include_result;
        include_result->source_name_length = path_str.size();
        include_result->source_name = source_name;
        include_result->content = content;
        include_result->content_length = file_content.size();
        return include_result;
    }
private:
    const std::vector<std::string> & m_search_paths;
};

ShaderCompiler::ShaderCompiler()
{
    m_include_directories.assign_range(sc::Config::instance().getIncludeDirectories() | stdv::transform([](const auto & dir) { return dir.string(); }));
}

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
    bool optimize) noexcept
{
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    for (const auto &macro_definition : m_macro_definitions) {
        options.AddMacroDefinition(macro_definition.c_str());
    }
    options.SetIncluder(std::make_unique<GlslShaderIncluder>(m_include_directories));
    if (optimize) { options.SetOptimizationLevel(shaderc_optimization_level_size); }
    const auto & entry_point = sc::Config::instance().getDefaultGlslEntryPoint();
    shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(
        source_code,
        enum_cast<shaderc_shader_kind>(type),
        shader_name.c_str(),
        entry_point.c_str(),
        options);
    if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
        lcf_log_error("shader compilation failed: {}", result.GetErrorMessage());
        return std::unexpected(std::make_error_code(std::errc::invalid_argument));
    }
    return spirv::Unit {type, {result.cbegin(), result.cend()}, entry_point};
}

std::expected<spirv::Unit, std::error_code> ShaderCompiler::compileGlslSourceToSpv(ShaderTypeFlagBits type, const std::filesystem::path &file_path) noexcept
{
    auto expected_file_content = read_file_as_string(sc::Config::instance().resolvePath(file_path));
    if (not expected_file_content) {
        lcf_log_error("failed to read file {}: {}", file_path.string(), expected_file_content.error().message());
        return std::unexpected(expected_file_content.error());
    }
    return this->compileGlslSourceToSpv(
        type,
        expected_file_content.value(),
        file_path.filename().string()); 
}

namespace {
    using Slang::ComPtr;

    slang::IGlobalSession & get_slang_global_session() noexcept
    {
        static ComPtr<slang::IGlobalSession> s_global_session_cp = []() {
            ComPtr<slang::IGlobalSession> session_cp;
            SlangResult sr = slang::createGlobalSession(session_cp.writeRef());
            if (SLANG_FAILED(sr)) {
                lcf_log_error("slang createGlobalSession failed: {}", sr);
            }
            return session_cp;
        }();
        return *s_global_session_cp;
    }

    std::expected<spirv::CompileResult, std::error_code> compile_slang(
        const std::string & source_code,
        const std::string & module_name,
        const std::vector<std::string> & include_directories)
    {
        auto & global_session = get_slang_global_session();

        const auto & slang_config = sc::Config::instance().getSlangConfig();
        slang::SessionDesc session_desc{};
        slang::TargetDesc target_desc{};
        target_desc.format = SLANG_SPIRV;
        target_desc.profile = global_session.findProfile(enum_name(slang_config.getTargetProfile()).data());
        session_desc.targets = &target_desc;
        session_desc.targetCount = 1;

        slang::CompilerOptionEntry entry_point_option{};
        entry_point_option.name = slang::CompilerOptionName::VulkanUseEntryPointName;
        entry_point_option.value.kind = slang::CompilerOptionValueKind::Int;
        entry_point_option.value.intValue0 = 1;
        session_desc.compilerOptionEntries = &entry_point_option;
        session_desc.compilerOptionEntryCount = 1;

        auto search_paths = include_directories | stdv::transform([](const auto & dir) { return dir.c_str(); }) | stdr::to<std::vector>();
        session_desc.searchPaths = search_paths.data();
        session_desc.searchPathCount = search_paths.size();

        ComPtr<slang::ISession> session_cp;
        global_session.createSession(session_desc, session_cp.writeRef());

        ComPtr<slang::IBlob> diag_cp;
        slang::IModule * slang_module = nullptr;
        try {
            slang_module = session_cp->loadModuleFromSourceString(
                module_name.c_str(),
                module_name.c_str(),
                source_code.c_str(),
                diag_cp.writeRef()
            );
        } catch (const std::exception & e) {
            lcf_log_error("slang loadModuleFromSourceString threw for '{}': {}", module_name, e.what());
            return std::unexpected(std::make_error_code(std::errc::operation_canceled));
        }
        if (not slang_module) {
            const char * msg = (diag_cp and diag_cp->getBufferPointer()) ? static_cast<const char *>(diag_cp->getBufferPointer()) : "(no diagnostic)";
            lcf_log_error("slang module load failed for '{}': {}", module_name, msg);
            return std::unexpected(std::make_error_code(std::errc::invalid_argument));
        }

        // 收集依赖文件路径 + 内容（用于参与 cache hash 计算）
        int32_t dep_count = slang_module->getDependencyFileCount();
        std::vector<std::string> dep_path_strs;
        std::vector<std::string> dep_contents;
        dep_path_strs.reserve(dep_count);
        dep_contents.reserve(dep_count);
        for (int32_t i = 0; i < dep_count; ++i) {
            const char * dep_path = slang_module->getDependencyFilePath(i);
            if (not dep_path) { continue; }
            auto expected_content = read_file_as_string(dep_path);
            if (not expected_content) { continue; }
            dep_path_strs.emplace_back(dep_path);
            dep_contents.emplace_back(std::move(expected_content.value()));
        }

        // 计算 cache hash
        std::vector<std::span<const std::byte>> chunks;
        chunks.reserve(2 + dep_path_strs.size() * 2 + 3);
        chunks.emplace_back(as_bytes(source_code));
        chunks.emplace_back(as_bytes(module_name));
        for (size_t i = 0; i < dep_path_strs.size(); ++i) {
            chunks.emplace_back(as_bytes(dep_path_strs[i]));
            chunks.emplace_back(as_bytes(dep_contents[i]));
        }
        chunks.emplace_back(as_bytes_from_value(slang_config.getTargetProfile()));
        chunks.emplace_back(as_bytes_from_value(slang_config.getCompilerOptionFlags()));
        chunks.emplace_back(as_bytes(slang_config.getVersion()));
        const uint64_t cache_hash = sc::hash(chunks);

        // composite + link + emit：slang COM 调用集中在此，统一保护
        int32_t ep_count = slang_module->getDefinedEntryPointCount();
        std::vector<ComPtr<slang::IEntryPoint>> entry_points;
        entry_points.reserve(ep_count);
        std::vector<slang::IComponentType *> components;
        components.reserve(ep_count + 1);
        components.emplace_back(slang_module);

        spirv::UnitList unit_list;
        try {
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

            for (int i = 0; i < ep_count; ++i) {
                ComPtr<slang::IBlob> spirv_blob;
                linked_cp->getEntryPointCode(i, 0, spirv_blob.writeRef(), diag_cp.writeRef());
                std::span<const uint32_t> data_span(static_cast<const uint32_t *>(spirv_blob->getBufferPointer()), spirv_blob->getBufferSize() / sizeof(uint32_t));
                auto * ep_reflection = linked_cp->getLayout()->getEntryPointByIndex(i);
                ShaderTypeFlagBits stage = enum_cast<ShaderTypeFlagBits>(ep_reflection->getStage());
                unit_list.emplace_back(stage, data_span | stdr::to<std::vector>(), ep_reflection->getName());
            }
        } catch (const std::exception & e) {
            lcf_log_error("slang link/emit threw for '{}': {}", module_name, e.what());
            return std::unexpected(std::make_error_code(std::errc::operation_canceled));
        }

        auto dep_paths = dep_path_strs | stdv::transform([](const auto & s) { return std::filesystem::path(s); }) | stdr::to<std::vector>();
        return spirv::CompileResult {std::move(unit_list), std::move(dep_paths), cache_hash};
    }
}

std::expected<spirv::UnitList, std::error_code> ShaderCompiler::compileSlangSourceToSpv(const std::string & source_code, const std::string & module_name) noexcept
{
    auto compiled = compile_slang(source_code, module_name, m_include_directories);
    if (not compiled) { return std::unexpected(compiled.error()); }
    return std::move(compiled->m_units);
}

std::expected<spirv::UnitList, std::error_code> ShaderCompiler::compileSlangSourceToSpv(const std::filesystem::path & file_path) noexcept
{
    auto resolved_path = sc::Config::instance().resolvePath(file_path);

    sc::spirv::ShaderCache cache;
    auto cached_opt = cache.tryLoad(resolved_path, {});
    if (cached_opt) { return std::move(*cached_opt); }

    
    auto expected_file_content = read_file_as_string(resolved_path);
    if (not expected_file_content) {
        lcf_log_error("failed to read file {}: {}", resolved_path.string(), expected_file_content.error().message());
        return std::unexpected(expected_file_content.error());
    }

    auto module_name = resolved_path.stem().string();
    auto compiled = compile_slang(expected_file_content.value(), module_name, m_include_directories);
    if (not compiled) { return std::unexpected(compiled.error()); }
    cache.store(resolved_path, {}, *compiled);
    
    return std::move(compiled->m_units);
}
