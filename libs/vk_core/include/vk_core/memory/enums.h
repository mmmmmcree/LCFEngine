#pragma once

#include <cstdint>

namespace lcf::vkc {

enum class MemoryAccess : uint8_t
{
    eDeviceLocal = 0,     // GPU-only, never mapped; CPU has no access
    eHostSequentialWrite, // persistently mapped, CPU writes only (uploads, per-frame UBO)
    eHostRandom,          // persistently mapped, CPU may read back
};

enum class AllocationStrategy : uint8_t
{
    eDefault = 0,
    eMinMemory, // minimise fragmentation / footprint
    eMinTime,   // minimise allocation time
};

} // namespace lcf::vkc
