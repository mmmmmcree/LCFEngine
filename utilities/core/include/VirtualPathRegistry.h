#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

namespace lcf {
    class VirtualPathRegistry
    {
        using Self = VirtualPathRegistry;
        using AliasToPathMap = std::unordered_map<std::string, std::filesystem::path>;
        static constexpr std::string_view k_separator = "://";
    public:
        static Self & instance() noexcept
        {
            static Self s_instance;
            return s_instance;
        }
    public:
        Self & registerAlias(std::string alias, std::filesystem::path real_path)
        {
            m_alias_to_path_map.insert_or_assign(std::move(alias), std::move(real_path));
            return *this;
        }
        std::filesystem::path resolve(const std::filesystem::path & path) const noexcept
        {
            auto str = path.string();
            auto sep_pos = str.find(k_separator);
            if (sep_pos == std::string::npos) { return path; }
            std::string alias = str.substr(0, sep_pos);
            auto it = m_alias_to_path_map.find(alias);
            if (it == m_alias_to_path_map.end()) { return path; }
            return it->second / std::filesystem::path(str.substr(sep_pos + k_separator.size()));
        }
    private:
        VirtualPathRegistry() = default;
    private:
        AliasToPathMap m_alias_to_path_map;
    };
}
