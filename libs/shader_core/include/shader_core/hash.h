#pragma once

#include <cstdint>
#include <span>

namespace lcf::sc {
    uint64_t hash(std::span<const std::span<const std::byte>> chunks) noexcept;

    uint64_t hash(std::span<const std::byte> chunk) noexcept;
}
