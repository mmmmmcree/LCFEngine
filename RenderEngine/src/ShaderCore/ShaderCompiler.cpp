#include "ShaderCompiler.h"
#include "ShaderIncluder.h"
#include <QDebug>
#include <QFile>
#include <QDir>

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
        auto includer = ShaderIncluder::makeUnique();
        for (const auto &include_directory : m_include_directories) {
            includer->addIncludeDirectory(include_directory.c_str());
        }
        options.SetIncluder(std::move(includer));
    }
    if (optimize) { options.SetOptimizationLevel(shaderc_optimization_level_size); }
    shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(source_code, enum_cast<shaderc_shader_kind>(type), shader_name, entry_point, options);
    if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
        qDebug() << "Shader compilation failed: " << result.GetErrorMessage();
        return {};
    }
    return {result.cbegin(), result.cend()};
}

lcf::SpvCode lcf::ShaderCompiler::compileGlslSourceFileToSpv(const char *file_path, ShaderTypeFlagBits type, const char *entry_point, bool optimize)
{
    QFile file(file_path);
    if (not file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Failed to open file:" << file_path;
        qDebug() << "Current working directory:" << QDir::currentPath();
        return {};
    }
    return this->compileGlslSourceToSpv(file.fileName().toLocal8Bit(), type, file.readAll().data(), entry_point, optimize);
}

lcf::ShaderResources lcf::ShaderCompiler::analyzeSpvCode(const SpvCode &spv_code)
{
    spirv_cross::Compiler spv_compiler(spv_code);
    spirv_cross::ShaderResources resources = spv_compiler.get_shader_resources();
    ShaderResources result;
    // for (const auto &resources : resources.stage_inputs) {
    //     ShaderResource shader_resource = this->parseResource(spv_compiler, resources);
    //     result.stage_inputs[shader_resource.getLocation()] = std::move(shader_resource);
    // }
    //! currently ,the parsing process of stage inputs and outputs is not implemented, if you parse them, the program will crash;
    for (const auto &resources : resources.uniform_buffers) {
        result.uniform_buffers.emplace_back(this->parseResource(spv_compiler, resources));
    }
    for (const auto &resources : resources.sampled_images) {
        result.sampled_images.emplace_back(this->parseResource(spv_compiler, resources));
    }
    for (const auto &resources : resources.storage_images) {
        result.storage_images.emplace_back(this->parseResource(spv_compiler, resources));
    }
    for (const auto &resources : resources.push_constant_buffers) {
        result.push_constant_buffers.emplace_back(this->parseResource(spv_compiler, resources));
    }
    return result;
}

lcf::ShaderResource lcf::ShaderCompiler::parseResource(const spirv_cross::Compiler &spv_compiler, const spirv_cross::Resource &resource)
{
    ShaderResource result;
    //! parse buffer like resource; for stage inputs and outputs, the parsing process is different
    result.setName(spv_compiler.get_name(resource.base_type_id))
        .setBinding(spv_compiler.get_decoration(resource.id, spv::DecorationBinding))
        .setSet(spv_compiler.get_decoration(resource.id, spv::DecorationDescriptorSet));
    this->parseResourceMembers(result, spv_compiler, resource.type_id);
    auto &members = result.getMembers();
    auto buffer_ranges = spv_compiler.get_active_buffer_ranges(resource.id);
    for (const auto &buffer_range : buffer_ranges) {
        members[buffer_range.index].setOffset(buffer_range.offset)
            .setSizeInBytes(buffer_range.range);
    }
    return result;
}

void lcf::ShaderCompiler::parseResourceMembers(ShaderResourceMember &resource, const spirv_cross::Compiler &spv_compiler, spirv_cross::TypeID type_id)
{
    const auto &type = spv_compiler.get_type(type_id);
    resource.setBaseDataType(type.basetype)
        .setWidth(type.width)
        .setColumns(type.columns)
        .setVecSize(type.vecsize)
        .setArraySize(type.array.size());
    if (type.basetype == spirv_cross::SPIRType::Struct) {
        resource.setSizeInBytes(spv_compiler.get_declared_struct_size(type));
    }
    uint32_t size = 0;
    for (int i = 0; i < type.member_types.size(); ++i) {
        const auto &member_type = spv_compiler.get_type(type.member_types[i]);
        ShaderResourceMember member;
        member.setOffset(spv_compiler.type_struct_member_offset(type, i))
            .setSizeInBytes(spv_compiler.get_declared_struct_member_size(type, i));
        this->parseResourceMembers(member, spv_compiler, type.member_types[i]);
        resource.addMember(member);
    }
}