#include "shader_core/config.h"
#include "shader_core/Manifest.h"
#include "VirtualPathRegistry.h"
#include <format>

using namespace lcf::sc;

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

std::filesystem::path Config::resolvePath(const std::filesystem::path &path) const noexcept
{
    return VirtualPathRegistry::instance().resolve(path);
}

#include <slang.h>

sl::Config::Config(
    TargetProfile target_profile,
    CompilerOptionFlags compiler_option_flags) :
    m_settings(target_profile, compiler_option_flags)
{
    auto tag = spGetBuildTagString();
    m_version = tag ? tag : "unknown";
}