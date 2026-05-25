#pragma once

#include <cstdint>
#include <span>

namespace lcf::shader_core {
    uint64_t hash(std::span<const std::span<const std::byte>> chunks) noexcept;
}
