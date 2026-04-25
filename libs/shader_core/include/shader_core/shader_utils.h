#pragma once

#include "ShaderResource.h"
#include "JSON.h"
#include <string_view>
#include "SpvCode.h"

namespace lcf::spirv {
    lcf::JSON extract_pragmas(std::string_view source_code);

    ShaderResources analyze(const Code & spv_code) noexcept;
}
