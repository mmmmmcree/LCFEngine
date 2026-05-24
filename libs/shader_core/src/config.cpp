#include "shader_core/config.h"
#include "VirtualPathRegistry.h"

using namespace lcf::shader_core;

auto Config::instance() noexcept -> Self &
{
    static Config s_instance;
    return s_instance;
}

auto Config::registerVirtualPath(std::string virtual_alias, std::filesystem::path real_path) noexcept -> Self &
{
    m_include_directories.emplace_back(real_path);
    lcf::VirtualPathRegistry::instance().registerAlias(std::move(virtual_alias), std::move(real_path));
    return *this;
}

auto Config::addIncludeDirectory(std::filesystem::path path) noexcept -> Self &
{
    m_include_directories.emplace_back(std::move(path));
    return *this;
}

auto Config::setCacheDirectory(std::filesystem::path path) noexcept -> Self &
{
    m_cache_directory = std::move(path);
    return *this;
}

auto Config::setDefaultGlslEntryPoint(std::string entry_point) noexcept -> Self &
{
    m_default_glsl_entry_point = std::move(entry_point);
    return *this;
}

std::filesystem::path lcf::shader_core::Config::resolvePath(const std::filesystem::path &path) const noexcept
{
    return lcf::VirtualPathRegistry::instance().resolve(path);
}
