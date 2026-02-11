#include "shader_core/ShaderCompiler.h"
#include "shader_core/shader_core_enums.h"
#include "shader_core/ShaderIncluder.h"
#include "log.h"
#include <filesystem>
#include <fstream>

void lcf::ShaderCompiler::addMacroDefinition(std::string_view macro_definition)
{
    m_macro_definitions.emplace_back(macro_definition);
}

void lcf::ShaderCompiler::addIncludeDirectory(std::string_view include_directory)
{
    m_include_directories.emplace_back(include_directory);
}

lcf::SpvCode lcf::ShaderCompiler::compileGlslSourceToSpv(const char *shader_name, ShaderTypeFlagBits type, const char *source_code, const char *entry_point, bool optimize)
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
    shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(source_code, enum_cast<shaderc_shader_kind>(type), shader_name, entry_point, options);
    if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
        std::runtime_error error(std::format("shader compilation failed: {}", result.GetErrorMessage()));
        lcf_log_error(error.what());
        throw error;
    }
    return {result.cbegin(), result.cend()};
}

lcf::SpvCode lcf::ShaderCompiler::compileGlslSourceFileToSpv(const std::filesystem::path & file_path, ShaderTypeFlagBits type, const char *entry_point, bool optimize)
{
    if (not std::filesystem::exists(file_path)) {
        std::runtime_error error(std::format("file {} not found", file_path.string()));
        lcf_log_error(error.what());
        throw error;
    }
    std::ifstream file(file_path, std::ios::in);
    if (not file.is_open()) {
        std::runtime_error error(std::format("failed to open file {}", file_path.string()));
        lcf_log_error(error.what());
        throw error;
    }
    std::string file_content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return this->compileGlslSourceToSpv(file_path.filename().string().c_str(), type, file_content.c_str(), entry_point, optimize);
}

lcf::ShaderResources lcf::ShaderCompiler::analyzeSpvCode(const SpvCode &spv_code)
{
    spirv_cross::Compiler spv_compiler(spv_code);
    spirv_cross::ShaderResources resources = spv_compiler.get_shader_resources();
    ShaderResources result;
    for (const auto &resources : resources.stage_inputs) {
        result.stage_inputs.emplace_back(this->parseInputResource(spv_compiler, resources));
    }
    std::ranges::sort(result.stage_inputs, [](const auto &a, const auto &b) { return a.getLocation() < b.getLocation(); });
    for (const auto &resources : resources.uniform_buffers) {
        result.uniform_buffers.emplace_back(this->parseBufferResource(spv_compiler, resources));
    }
    for (const auto &resources : resources.storage_buffers) {
        result.storage_buffers.emplace_back(this->parseBufferResource(spv_compiler, resources));
    }
    for (const auto &resources : resources.sampled_images) {
        result.sampled_images.emplace_back(this->parseResource(spv_compiler, resources));
    }
    for (const auto &resources : resources.separate_images) {
        result.separate_images.emplace_back(this->parseResource(spv_compiler, resources));
    }
    for (const auto &resources : resources.separate_samplers) {
        result.separate_samplers.emplace_back(this->parseResource(spv_compiler, resources));
    }
    for (const auto &resources : resources.storage_images) {
        result.storage_images.emplace_back(this->parseResource(spv_compiler, resources));
    }
    for (const auto &resources : resources.push_constant_buffers) {
        result.push_constant_buffers.emplace_back(this->parseResource(spv_compiler, resources));
    }
    return result;
}

lcf::ShaderResource lcf::ShaderCompiler::parseBufferResource(const spirv_cross::Compiler &spv_compiler, const spirv_cross::Resource &resource)
{
    auto result = this->parseResource(spv_compiler, resource);
    result.setName(spv_compiler.get_name(resource.base_type_id));
    auto &members = result.getMembers();
    auto buffer_ranges = spv_compiler.get_active_buffer_ranges(resource.id);
    for (const auto &buffer_range : buffer_ranges) {
        members[buffer_range.index].setOffset(buffer_range.offset)
            .setSizeInBytes(buffer_range.range);
    }
    return result;
}

lcf::ShaderResource lcf::ShaderCompiler::parseResource(const spirv_cross::Compiler &spv_compiler, const spirv_cross::Resource &resource)
{
    ShaderResource result;
    //! parse buffer like resource; for stage inputs and outputs, the parsing process is different
    result.setBinding(spv_compiler.get_decoration(resource.id, spv::DecorationBinding))
        .setSet(spv_compiler.get_decoration(resource.id, spv::DecorationDescriptorSet));
    this->parseResourceMembers(result, spv_compiler, resource.type_id);
    result.setName(spv_compiler.get_name(resource.id));
    return result;
}

void lcf::ShaderCompiler::parseResourceMembers(ShaderResourceMember &resource, const spirv_cross::Compiler &spv_compiler, spirv_cross::TypeID type_id)
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
        this->parseResourceMembers(member, spv_compiler, type.member_types[i]);
        resource.addMember(member);
    }
}

lcf::ShaderResource lcf::ShaderCompiler::parseInputResource(const spirv_cross::Compiler &spv_compiler, const spirv_cross::Resource &resource)
{
    ShaderResource result;
    result.setLocation(spv_compiler.get_decoration(resource.id, spv::DecorationLocation))
        .setBinding(spv_compiler.get_decoration(resource.id, spv::DecorationBinding));
    this->parseInputResourceMembers(result, spv_compiler, resource.type_id);
    result.setName(spv_compiler.get_name(resource.base_type_id));
    return result;
}

void lcf::ShaderCompiler::parseInputResourceMembers(ShaderResourceMember &resource, const spirv_cross::Compiler &spv_compiler, spirv_cross::TypeID type_id)
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
        this->parseInputResourceMembers(member, spv_compiler, type.member_types[i]);
        member.setName(spv_compiler.get_member_name(type.self, i))
            .setOffset(size);
        resource.addMember(member);
        size += member.getSizeInBytes();
    }
    resource.setSizeInBytes(size);
}
