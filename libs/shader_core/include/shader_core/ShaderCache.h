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
        ShaderCache() noexcept = default;
        ShaderCache(const ShaderCache&) = delete;
        ShaderCache& operator=(const ShaderCache&) = delete;
        ShaderCache(ShaderCache &&) = delete;
        ShaderCache& operator=(ShaderCache &&) = delete;
    public:
        std::optional<spirv::UnitList> tryLoad(const std::filesystem::path & source_path, const std::string & compile_command) const noexcept;
        void store(const std::filesystem::path & source_path, const std::string & compile_command, const CompileResult & compile_result) const noexcept;
    };
}
