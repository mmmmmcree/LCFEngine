#pragma once

#include "shader_core_enums.h"
#include <cstdint>
#include <vector>
#include <string>
#include <filesystem>

namespace lcf::sc::spirv {
    using Code = std::vector<uint32_t>;

    struct Unit
    {
        ~Unit() noexcept = default;
        Unit() = default;
        Unit(ShaderTypeFlagBits stage, Code code, std::string entry_point) :
            m_stage(stage), m_code(std::move(code)), m_entry_point(std::move(entry_point)) {}
        const ShaderTypeFlagBits & getStage() const { return m_stage; }
        const Code & getCode() const { return m_code; }
        const std::string & getEntryPoint() const { return m_entry_point; }

        ShaderTypeFlagBits m_stage;
        Code m_code;
        std::string m_entry_point;
    };

    using UnitList = std::vector<Unit>;

    struct CompileResult
    {
        using DependencyPathList = std::vector<std::filesystem::path>;
        CompileResult() = default;
        CompileResult(
            spirv::UnitList units,
            DependencyPathList dep_paths,
            uint64_t cache_hash) noexcept :
            m_units(std::move(units)),
            m_dep_paths(std::move(dep_paths)),
            m_cache_hash(cache_hash) {}
        const spirv::UnitList & getUnits() const noexcept { return m_units; }
        const DependencyPathList & getDependencyPaths() const noexcept { return m_dep_paths; }
        const uint64_t & getCacheHash() const noexcept { return m_cache_hash; }

        spirv::UnitList m_units;
        DependencyPathList m_dep_paths;
        uint64_t m_cache_hash = 0;
    };
}