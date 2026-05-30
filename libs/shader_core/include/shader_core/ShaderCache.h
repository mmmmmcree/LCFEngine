#pragma once

#include "spirv.h"
#include <filesystem>
#include <optional>
#include <system_error>

namespace lcf::sc::spirv {
    class ShaderCache
    {
    public:
        std::optional<spirv::UnitList> tryLoad(uint64_t hash) const noexcept;
        std::error_code store(uint64_t hash, const spirv::UnitList & units) const noexcept;
    };
}
