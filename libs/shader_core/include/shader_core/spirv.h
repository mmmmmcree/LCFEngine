#pragma once

#include "shader_core_enums.h"
#include <cstdint>
#include <vector>
#include <string>

namespace lcf::spirv {
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
}