#pragma once

#include <filesystem>
#include <string>

namespace lcf::ra {
    class Config
    {
        using Self = Config;
    public:
        static Self & instance() noexcept;
    public:
        ~Config() noexcept = default;
    private:
        Config() = default;
    public:
        Self & registerVirtualPath(std::string virtual_alias, std::filesystem::path real_path) noexcept;
        std::filesystem::path resolvePath(const std::filesystem::path & path) const noexcept;
    };
}