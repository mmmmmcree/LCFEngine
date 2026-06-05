#include "render_assets/configs/config.h"
#include "VirtualPathRegistry.h"

using namespace lcf::ra;

auto Config::instance() noexcept -> Self &
{
    static Config s_instance;
    return s_instance;
}

auto Config::registerVirtualPath(std::string virtual_alias, std::filesystem::path real_path) noexcept -> Self &
{
    lcf::VirtualPathRegistry::instance().registerAlias(std::move(virtual_alias), std::move(real_path));
    return *this;
}

std::filesystem::path Config::resolvePath(const std::filesystem::path &path) const noexcept
{
    return VirtualPathRegistry::instance().resolve(path);
}