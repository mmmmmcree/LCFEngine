#pragma once

#include "spirv.h"
#include <filesystem>
#include <optional>
#include <system_error>

namespace lcf::sc {
    class Manifest;
}

namespace lcf::sc::spirv {
    class ShaderCache
    {
    public:
        ~ShaderCache() noexcept = default;
        // ShaderCache();
        // ShaderCache(const ShaderCache&) = delete;
        // ShaderCache& operator=(const ShaderCache&) = delete;
        // ShaderCache(ShaderCache &&) = delete;
        // ShaderCache& operator=(ShaderCache &&) = delete;
    public:
        std::optional<spirv::UnitList> tryLoad(uint64_t hash) const noexcept;
        std::error_code store(uint64_t hash, const spirv::UnitList & units) const noexcept;
    private:
        Manifest & getManifestInstance() const noexcept;
    private:
        std::filesystem::path m_cache_directory;
    };
}
