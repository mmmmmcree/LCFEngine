#pragma once

#include "ShaderResource.h"
#include "JSON.h"
#include <cstdint>
#include <string_view>
#include <vector>

namespace lcf::spirv {
    using Code = std::vector<uint32_t>;

    lcf::JSON extract_pragmas(std::string_view source_code);

    ShaderResources analyze(const Code & spv_code) noexcept;
}
