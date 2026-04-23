#include "shader_core/ShaderCompiler.h"
#include "shader_core/shader_core_enums.h"
#include "shader_core/ShaderIncluder.h"
#include "log.h"
#include <filesystem>
#include <fstream>
#include <ranges>
#include <regex>

using namespace lcf;
namespace stdr = std::ranges;

void lcf::ShaderCompiler::addMacroDefinition(std::string_view macro_definition)
{
    m_macro_definitions.emplace_back(macro_definition);
}

void lcf::ShaderCompiler::addIncludeDirectory(std::string_view include_directory)
{
    m_include_directories.emplace_back(include_directory);
}

lcf::SpvCode lcf::ShaderCompiler::compileGlslSourceToSpv(
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

lcf::JSON lcf::extract_pragmas(std::string_view source_code)
{
    lcf::JSON result = lcf::JSON::object();
    static const std::regex pragma_pattern(R"(#\s*pragma\s+lcf\s+(\w+)\s*\(\s*([^)]*)\s*\)\s*;?)");
    static const std::regex kv_pattern(R"((\w+)\s*=\s*([\w\d_]+))");
    auto begin = std::cregex_iterator(source_code.data(), source_code.data() + source_code.size(), pragma_pattern);
    for (const auto& match : std::ranges::subrange(begin, std::cregex_iterator{})) {
        auto pragma_name = match[1].str();
        auto pragma_args_str = match[2].str();
        auto args = lcf::JSON::object();
        auto kv_begin = std::sregex_iterator(pragma_args_str.begin(), pragma_args_str.end(), kv_pattern);
        for (const auto& kv_match : std::ranges::subrange(kv_begin, std::sregex_iterator{})) {
            args[kv_match[1].str()] = kv_match[2].str();
        }
        result[pragma_name].emplace_back(std::move(args));
    }
    return result;   
}

static ShaderResource parse_buffer_resource(const spirv_cross::Compiler &spv_compiler, const spirv_cross::Resource &resource) noexcept;

static ShaderResource parse_resource(const spirv_cross::Compiler &spv_compiler, const spirv_cross::Resource &resource) noexcept;

static void parse_resource_members(ShaderResourceMember &resource, const spirv_cross::Compiler &spv_compiler, spirv_cross::TypeID type_id) noexcept;

static ShaderResource parse_input_resource(const spirv_cross::Compiler &spv_compiler, const spirv_cross::Resource &resource) noexcept;

static void parse_input_resource_members(ShaderResourceMember &resource, const spirv_cross::Compiler &spv_compiler, spirv_cross::TypeID type_id) noexcept;

ShaderResources lcf::analyze_spv_code(const SpvCode &spv_code) noexcept
{
    spirv_cross::Compiler spv_compiler(spv_code);
    spirv_cross::ShaderResources resources = spv_compiler.get_shader_resources();
    ShaderResources result;
    for (const auto &resources : resources.stage_inputs) {
        result.stage_inputs.emplace_back(parse_input_resource(spv_compiler, resources));
    }
    std::ranges::sort(result.stage_inputs, [](const auto &a, const auto &b) { return a.getLocation() < b.getLocation(); });
    for (const auto &resources : resources.uniform_buffers) {
        result.uniform_buffers.emplace_back(parse_buffer_resource(spv_compiler, resources));
    }
    for (const auto &resources : resources.storage_buffers) {
        result.storage_buffers.emplace_back(parse_buffer_resource(spv_compiler, resources));
    }
    for (const auto &resources : resources.sampled_images) {
        result.sampled_images.emplace_back(parse_resource(spv_compiler, resources));
    }
    for (const auto &resources : resources.separate_images) {
        result.separate_images.emplace_back(parse_resource(spv_compiler, resources));
    }
    for (const auto &resources : resources.separate_samplers) {
        result.separate_samplers.emplace_back(parse_resource(spv_compiler, resources));
    }
    for (const auto &resources : resources.storage_images) {
        result.storage_images.emplace_back(parse_resource(spv_compiler, resources));
    }
    for (const auto &resources : resources.push_constant_buffers) {
        result.push_constant_buffers.emplace_back(parse_resource(spv_compiler, resources));
    }
    return result;
}

ShaderResource parse_buffer_resource(const spirv_cross::Compiler &spv_compiler, const spirv_cross::Resource &resource) noexcept
{
    auto result = parse_resource(spv_compiler, resource);
    result.setName(spv_compiler.get_name(resource.base_type_id));
    auto &members = result.getMembers();
    auto buffer_ranges = spv_compiler.get_active_buffer_ranges(resource.id);
    for (const auto &buffer_range : buffer_ranges) {
        members[buffer_range.index].setOffset(buffer_range.offset)
            .setSizeInBytes(buffer_range.range);
    }
    return result;
}

ShaderResource parse_resource(const spirv_cross::Compiler &spv_compiler, const spirv_cross::Resource &resource) noexcept
{
    ShaderResource result;
    //! parse buffer like resource; for stage inputs and outputs, the parsing process is different
    result.setBinding(spv_compiler.get_decoration(resource.id, spv::DecorationBinding))
        .setSet(spv_compiler.get_decoration(resource.id, spv::DecorationDescriptorSet));
    parse_resource_members(result, spv_compiler, resource.type_id);
    result.setName(spv_compiler.get_name(resource.id));
    return result;
}

void parse_resource_members(ShaderResourceMember &resource, const spirv_cross::Compiler &spv_compiler, spirv_cross::TypeID type_id) noexcept
{
    const auto &type = spv_compiler.get_type(type_id);
    size_t array_size = 1;
    for (auto size : type.array) { array_size *= size; }
    resource.setBaseDataType(enum_cast<ShaderDataType>(type.basetype))
        .setWidth(type.width)
        .setColumns(type.columns)
        .setVecSize(type.vecsize)
        .setArraySize(array_size);
    if (type.basetype == spirv_cross::SPIRType::Struct) {
        resource.setSizeInBytes(spv_compiler.get_declared_struct_size(type));
    }
    if (type.member_types.empty()) { return; } // if empty, the size of the data structure is undefined
    for (int i = 0; i < type.member_types.size(); ++i) {
        const auto &member_type = spv_compiler.get_type(type.member_types[i]);
        ShaderResourceMember member;
        member.setName(spv_compiler.get_member_name(type.self, i))
            .setOffset(spv_compiler.type_struct_member_offset(type, i))
            .setSizeInBytes(spv_compiler.get_declared_struct_member_size(type, i));
        parse_resource_members(member, spv_compiler, type.member_types[i]);
        resource.addMember(member);
    }
}

ShaderResource parse_input_resource(const spirv_cross::Compiler &spv_compiler, const spirv_cross::Resource &resource) noexcept
{
    ShaderResource result;
    result.setLocation(spv_compiler.get_decoration(resource.id, spv::DecorationLocation))
        .setBinding(spv_compiler.get_decoration(resource.id, spv::DecorationBinding));
    parse_input_resource_members(result, spv_compiler, resource.type_id);
    result.setName(spv_compiler.get_name(resource.base_type_id));
    return result;
}

void parse_input_resource_members(ShaderResourceMember &resource, const spirv_cross::Compiler &spv_compiler, spirv_cross::TypeID type_id) noexcept
{
    const auto &type = spv_compiler.get_type(type_id);
    resource.setBaseDataType(enum_cast<ShaderDataType>(type.basetype))
        .setWidth(type.width)
        .setColumns(type.columns)
        .setVecSize(type.vecsize)
        .setArraySize(type.array.size());
    uint32_t size = resource.getWidth() / 8u * resource.getVecSize() * resource.getColumns() * resource.getArraySize();
    for (int i = 0; i < type.member_types.size(); ++i) {
        ShaderResource member;
        parse_input_resource_members(member, spv_compiler, type.member_types[i]);
        member.setName(spv_compiler.get_member_name(type.self, i))
            .setOffset(size);
        resource.addMember(member);
        size += member.getSizeInBytes();
    }
    resource.setSizeInBytes(size);
}
