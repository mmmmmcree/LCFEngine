#pragma once

#include <string>
#include <string_view>
#include <filesystem>
#include <vector>
#include <unordered_map>
#include <span>

namespace lcf::shader_core {
    class Config
    {
        using Self = Config;
        using IncludeDirectoryList = std::vector<std::filesystem::path>;
    public:
        static Self & instance() noexcept;
    public:
        ~Config() noexcept = default;
        Config() = default;
    public:
        Self & registerVirtualPath(std::string virtual_alias, std::filesystem::path real_path) noexcept;
        Self & addIncludeDirectory(std::filesystem::path path) noexcept;
        Self & setDefaultGlslEntryPoint(std::string entry_point) noexcept;
        const std::string & getDefaultGlslEntryPoint() const noexcept { return m_default_glsl_entry_point; }
        std::filesystem::path resolvePath(const std::filesystem::path & path) const noexcept;
        std::span<const std::filesystem::path> getIncludeDirectories() const noexcept { return m_include_directories; }
    private:
        IncludeDirectoryList m_include_directories;
        std::string m_default_glsl_entry_point = "main";
    };
}