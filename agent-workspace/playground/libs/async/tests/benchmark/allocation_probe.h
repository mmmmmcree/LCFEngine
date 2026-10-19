#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace lcf::async::comparison::allocation {

struct Snapshot
{
    std::uint64_t m_allocations;
    std::uint64_t m_bytes;
    std::uint64_t m_peak_bytes;
    std::uint64_t m_live_bytes;
};

void begin() noexcept;
Snapshot end() noexcept;

}
