#include "shader_core/ShaderIncluder.h"
#include <filesystem>
#include <fstream>

void lcf::ShaderIncluder::addIncludeDirectory(const char *path)
{
    m_search_paths.push_back(path);
}

shaderc_include_result *lcf::ShaderIncluder::GetInclude(const char *requested_source, shaderc_include_type type, const char *requesting_source, size_t include_depth)
{
    if (type == shaderc_include_type_relative) {
        auto requesting_dir = std::filesystem::path(requested_source).parent_path();
        auto full_path = requesting_dir / requested_source;
        if (std::filesystem::exists(full_path)) {
            return this->createIncludeResult(full_path.string().c_str());
        }
    }
    for (const auto &path : m_search_paths) {
        auto full_path = std::filesystem::path(path) / requested_source;
        if (not std::filesystem::exists(full_path)) { continue; }
        return this->createIncludeResult(full_path.string().c_str());
    }
    return nullptr;
}

void lcf::ShaderIncluder::ReleaseInclude(shaderc_include_result *include_result)
{
    delete[] include_result->source_name;
    delete[] include_result->content;
    delete include_result;
}

shaderc_include_result *lcf::ShaderIncluder::createIncludeResult(const char *path)
{
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (not file.is_open()) { return nullptr; }
    shaderc_include_result* include_result = new shaderc_include_result;
    include_result->source_name_length = strlen(path);
    include_result->source_name = new char[include_result->source_name_length + 1];
    std::strcpy(const_cast<char*>(include_result->source_name), path);
    file.seekg(0, std::ios::end);
    size_t file_size = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);
    char* content = new char[file_size + 1];
    file.read(content, file_size);
    content[file_size] = '\0';
    include_result->content = content;
    include_result->content_length = file_size;
    file.close();
    return include_result;
}
