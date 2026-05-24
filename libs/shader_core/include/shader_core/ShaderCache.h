#pragma once

#include "spirv.h"
#include <filesystem>
#include <optional>

namespace lcf::shader_core {
    class ShaderCache
    {
    public:
        ~ShaderCache() noexcept = default;
        explicit ShaderCache(std::filesystem::path cache_dir);
    public:
        std::optional<spirv::UnitList> tryLoad(uint64_t hash) const noexcept;
        bool store(uint64_t hash, const spirv::UnitList & units) const noexcept;
    private:
        std::filesystem::path m_cache_dir;
    };
}
