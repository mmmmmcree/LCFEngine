#pragma once

#include "ShaderResource.h"
#include "JSON.h"
#include <string_view>
#include "spirv.h"

namespace lcf::sc::spirv {
    lcf::JSON extract_pragmas(std::string_view source_code);

    ShaderResources analyze(const Code & spv_code) noexcept;
}
