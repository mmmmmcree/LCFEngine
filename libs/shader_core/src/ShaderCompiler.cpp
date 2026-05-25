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
    m_include_directories.assign_range(shader_core::Config::instance().getIncludeDirectories() | stdv::transform([](const auto & dir) { return dir.string(); }));
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
    const auto & entry_point = shader_core::Config::instance().getDefaultGlslEntryPoint();
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
    auto expected_file_content = read_file_as_string(shader_core::Config::instance().resolvePath(file_path));
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

    struct SlangCompileResult
    {
        spirv::UnitList units;
        std::vector<std::filesystem::path> dep_paths;
        uint64_t cache_hash = 0;
    };

    std::expected<SlangCompileResult, std::error_code> compile_slang(
        const std::string & source_code,
        const std::string & module_name,
        const std::vector<std::string> & include_directories) noexcept
    {
        auto & global_session = get_slang_global_session();

        const auto & slang_config = shader_core::Config::instance().getSlangConfig();
        slang::SessionDesc session_desc{};
        std::vector<slang::TargetDesc> target_decsc;
        auto & target_desc = target_decsc.emplace_back();
        target_desc.format = SLANG_SPIRV;
        target_desc.profile = global_session.findProfile(enum_name(slang_config.getTargetProfile()).data());
        session_desc.targets = target_decsc.data();
        session_desc.targetCount = target_decsc.size();

        std::vector<slang::CompilerOptionEntry> compiler_options;
        auto & entry_point_option = compiler_options.emplace_back();
        entry_point_option.name = slang::CompilerOptionName::VulkanUseEntryPointName;
        entry_point_option.value.kind = slang::CompilerOptionValueKind::Int;
        entry_point_option.value.intValue0 = 1;
        session_desc.compilerOptionEntries = compiler_options.data();
        session_desc.compilerOptionEntryCount = compiler_options.size();

        auto search_paths = include_directories | stdv::transform([](const auto & dir) { return dir.c_str(); }) | stdr::to<std::vector>();
        session_desc.searchPaths = search_paths.data();
        session_desc.searchPathCount = search_paths.size();

        ComPtr<slang::ISession> session_cp;
        global_session.createSession(session_desc, session_cp.writeRef());

        ComPtr<slang::IBlob> diag_cp;
        auto * slang_module = session_cp->loadModuleFromSourceString(
            module_name.c_str(),
            module_name.c_str(),
            source_code.c_str(),
            diag_cp.writeRef()
        );
        if (not slang_module) {
            const char * msg = (diag_cp and diag_cp->getBufferPointer()) ? static_cast<const char *>(diag_cp->getBufferPointer()) : "(no diagnostic)";
            lcf_log_error("slang module load failed: {}", msg);
            return std::unexpected(std::make_error_code(std::errc::invalid_argument));
        }

        //-- slang collect deps --
        int32_t dep_count = slang_module->getDependencyFileCount();
        std::vector<std::string> dep_path_strs; dep_path_strs.reserve(dep_count);
        std::vector<std::string> dep_contents; dep_contents.reserve(dep_count);
        for (int32_t i = 0; i < dep_count; ++i) {
            const char * dep_path = slang_module->getDependencyFilePath(i);
            if (not dep_path) { continue; }
            auto expected_content = read_file_as_string(dep_path);
            if (not expected_content) { continue; }
            dep_path_strs.emplace_back(dep_path);
            dep_contents.emplace_back(std::move(expected_content.value()));
        }

        std::vector<std::span<const std::byte>> chunks; chunks.reserve(2 + dep_path_strs.size() * 2 + 3);
        chunks.emplace_back(as_bytes(source_code));
        chunks.emplace_back(as_bytes(module_name));
        for (size_t i = 0; i < dep_path_strs.size(); ++i) {
            chunks.emplace_back(as_bytes(dep_path_strs[i]));
            chunks.emplace_back(as_bytes(dep_contents[i]));
        }
        chunks.emplace_back(as_bytes_from_value(slang_config.getTargetProfile()));
        chunks.emplace_back(as_bytes_from_value(slang_config.getCompilerOptionFlags()));
        chunks.emplace_back(as_bytes(slang_config.getVersion()));
        uint64_t cache_hash = shader_core::hash(chunks);

        // ---- slang compile ----
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

        SlangCompileResult result;
        result.units = std::move(unit_list);
        result.cache_hash = cache_hash;
        result.dep_paths.reserve(dep_path_strs.size());
        for (const auto & p : dep_path_strs) {
            result.dep_paths.emplace_back(p);
        }
        return result;
    }
}

std::expected<spirv::UnitList, std::error_code> ShaderCompiler::compileSlangSourceToSpv(const std::string & source_code, const std::string & module_name) noexcept
{
    auto compiled = compile_slang(source_code, module_name, m_include_directories);
    if (not compiled) { return std::unexpected(compiled.error()); }

    shader_core::ShaderCache cache;
    if (auto cached = cache.tryLoad(compiled->cache_hash)) {
        return std::move(*cached);
    }
    if (auto ec = cache.store(compiled->cache_hash, compiled->units)) {
        lcf_log_warn("slang cache store failed for '{}': {}", module_name, ec.message());
    }
    return std::move(compiled->units);
}

std::expected<spirv::UnitList, std::error_code> ShaderCompiler::compileSlangSourceToSpv(const std::filesystem::path & file_path) noexcept
{
    namespace sc = shader_core;
    using sc::Manifest;
    using sc::ManifestEntry;
    using sc::ShaderFingerprint;

    auto resolved = sc::Config::instance().resolvePath(file_path);
    auto canonical = sc::normalize_manifest_path(file_path);

    // ---- 快路径：查 manifest ----
    auto & manifest = Manifest::instance();
    if (const ManifestEntry * entry = manifest.find(canonical)) {
        const auto & current_settings = sc::Config::instance().getSlangConfig().getCompileSettings();
        bool config_match = (entry->m_compile_settings == current_settings);
        bool main_match = entry->m_main_fingerprint.matches(canonical);
        bool deps_match = stdr::all_of(entry->m_deps, [](const auto & dep) { return dep.second.matches(dep.first); });
        if (config_match and main_match and deps_match and not entry->m_products.empty()) {
            sc::ShaderCache cache;
            if (auto cached = cache.tryLoad(entry->m_products.front().m_compile_input_hash)) {
                lcf_log_info("shader manifest HIT '{}'", canonical.string());
                return std::move(*cached);
            }
            lcf_log_warn("shader manifest entry HIT but spvbin missing for '{}', falling back to MISS", canonical.string());
        }
    }

    // ---- 慢路径：MISS，走完整 slang 编译 ----
    lcf_log_info("shader manifest MISS '{}'", canonical.string());

    auto expected_file_content = read_file_as_string(resolved);
    if (not expected_file_content) {
        lcf_log_error("failed to read file {}: {}", file_path.string(), expected_file_content.error().message());
        return std::unexpected(expected_file_content.error());
    }
    auto module_name = file_path.stem().string();
    auto compiled = compile_slang(expected_file_content.value(), module_name, m_include_directories);
    if (not compiled) { return std::unexpected(compiled.error()); }

    // 写 ShaderCache（产物存储）
    sc::ShaderCache cache;
    if (auto ec = cache.store(compiled->cache_hash, compiled->units)) {
        lcf_log_warn("slang cache store failed for '{}': {}", module_name, ec.message());
    }

    // 写 Manifest（路径索引）
    ManifestEntry new_entry(canonical, sc::Config::instance().getSlangConfig().getCompileSettings());

    // dep 列表：对每个 slang 报告的 dep 做 weakly_canonical，过滤掉与主源相同的项。
    new_entry.m_deps.reserve(compiled->dep_paths.size());
    for (const auto & dep_raw : compiled->dep_paths) {
        std::error_code ec;
        auto dep_canonical = std::filesystem::weakly_canonical(dep_raw, ec);
        if (ec) { dep_canonical = dep_raw; }
        if (dep_canonical == canonical) { continue; }  // 主源自己不算依赖
        new_entry.m_deps.emplace_back(dep_canonical, ShaderFingerprint(dep_canonical));
    }

    sc::ProductRef prod;
    prod.m_compile_input_hash = compiled->cache_hash;
    new_entry.m_products.push_back(std::move(prod));

    manifest.upsert(std::move(new_entry));

    return std::move(compiled->units);
}
