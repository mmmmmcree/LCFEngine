#include "shader_core/hash.h"
#include "bytes.h"

#define XXH_INLINE_ALL
#include <xxhash.h>

uint64_t lcf::shader_core::hash(std::span<const std::span<const std::byte>> chunks) noexcept
{
    XXH3_state_t * state = XXH3_createState();
    XXH3_64bits_reset(state);
    for (const auto & chunk : chunks) {
        if (chunk.empty()) { continue; }
        XXH3_64bits_update(state, chunk.data(), chunk.size());
    }
    uint64_t result = XXH3_64bits_digest(state);
    XXH3_freeState(state);
    return result;
}

uint64_t lcf::shader_core::hash(std::span<const std::byte> chunk) noexcept
{
    return hash(std::span<decltype(chunk)>(&chunk, 1));
}