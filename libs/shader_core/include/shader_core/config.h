#pragma once

#include "shader_core_enums.h"
#include <string>
#include <string_view>
#include <filesystem>
#include <vector>
#include <unordered_map>
#include <span>
#include <system_error>

namespace lcf::sc {

namespace sl {  // slang namespace

    class CompileSettings
    {
        using Self = CompileSettings;
    public:
        CompileSettings() noexcept = default;
        CompileSettings(TargetProfile profile, CompilerOptionFlags flags) noexcept
            : m_target_profile(profile), m_compiler_option_flags(flags) {}

        bool operator==(const Self & other) const noexcept = default;

        Self & setTargetProfile(TargetProfile profile) noexcept { m_target_profile = profile; return *this; }
        Self & setCompilerOptionFlags(CompilerOptionFlags flags) noexcept { m_compiler_option_flags = flags; return *this; }

        const TargetProfile & getTargetProfile() const noexcept { return m_target_profile; }
        const CompilerOptionFlags & getCompilerOptionFlags() const noexcept { return m_compiler_option_flags; }

    public:
        TargetProfile m_target_profile = TargetProfile::eInvalid;
        CompilerOptionFlags m_compiler_option_flags = CompilerOptionFlags::eNone;
    };

    class Config
    {
        using Self = Config;
    public:
        Config(
            TargetProfile target_profile = TargetProfile::spirv_1_5,
            CompilerOptionFlags compiler_option_flags = CompilerOptionFlags::eVulkanUseEntryPointName
        );
    public:
        Self & setTargetProfile(TargetProfile profile) noexcept { m_settings.setTargetProfile(profile); return *this; }
        Self & setCompilerOptionFlags(CompilerOptionFlags flags) noexcept { m_settings.setCompilerOptionFlags(flags); return *this; }
        Self & setCompileSettings(CompileSettings s) noexcept { m_settings = s; return *this; }
        Self & setSpvCacheExtension(std::filesystem::path extension) noexcept { m_spv_cache_extension = extension; return *this; }

        const std::string & getVersion() const noexcept { return m_version; }
        const TargetProfile & getTargetProfile() const noexcept { return m_settings.getTargetProfile(); }
        const CompilerOptionFlags & getCompilerOptionFlags() const noexcept { return m_settings.getCompilerOptionFlags(); }
        const CompileSettings & getCompileSettings() const noexcept { return m_settings; }
        const std::filesystem::path & getSpvCacheExtension() const noexcept { return m_spv_cache_extension; }
    private:
        std::string m_version;
        CompileSettings m_settings;
        std::filesystem::path m_spv_cache_extension = ".spvbin";
    };
}
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
        Self & setCacheDirectory(std::filesystem::path path) noexcept;
        Self & setDefaultGlslEntryPoint(std::string entry_point) noexcept;
        const std::string & getDefaultGlslEntryPoint() const noexcept { return m_default_glsl_entry_point; }
        std::filesystem::path resolvePath(const std::filesystem::path & path) const noexcept;
        std::span<const std::filesystem::path> getIncludeDirectories() const noexcept { return m_include_directories; }
        const std::filesystem::path & getCacheDirectory() const noexcept { return m_cache_directory; }
        sl::Config & getSlangConfig() noexcept { return m_slang_config; }
        const sl::Config & getSlangConfig() const noexcept { return m_slang_config; }
        std::filesystem::path makeSpvCachePath(uint64_t hash) const noexcept;
    private:
        IncludeDirectoryList m_include_directories;
        std::filesystem::path m_cache_directory = ".shader_cache";
        std::string m_default_glsl_entry_point = "main";
        sl::Config m_slang_config;
    };
}
