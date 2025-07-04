#include "ShaderIncluder.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>

void lcf::ShaderIncluder::addIncludeDirectory(const char *path)
{
    m_search_paths.push_back(path);
}

shaderc_include_result *lcf::ShaderIncluder::GetInclude(const char *requested_source, shaderc_include_type type, const char *requesting_source, size_t include_depth)
{
    if (type == shaderc_include_type_relative) {
        QString requesting_dir = QFileInfo(requested_source).path();
        QString full_path = QDir(requesting_dir).filePath(requested_source);
        if (QFile::exists(full_path)) {
            return this->createIncludeResult(full_path.toLocal8Bit().constData());
        }
    }
    for (const auto &path : m_search_paths) {
        QString full_path = QDir(path.c_str()).filePath(requested_source);
        if (not QFile::exists(full_path)) { continue; }
        return this->createIncludeResult(full_path.toLocal8Bit().constData());
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
    QFile file(path);
    if (not file.open(QIODevice::ReadOnly | QIODevice::Text)) { return nullptr; }
    shaderc_include_result *include_result = new shaderc_include_result;
    char *source_name = new char[strlen(path) + 1];
    strcpy(source_name, path);
    include_result->source_name = source_name;
    include_result->source_name_length = strlen(source_name);
    QByteArray data = file.readAll();
    char *content = new char[data.size() + 1];
    strcpy(content, data.constData());
    include_result->content = content;
    include_result->content_length = data.size();
    return include_result;
}
