#pragma once

#include <xmemory>

namespace lcf {
    bool is_multiple_of(uint32_t value, uint32_t multiple);

    uint32_t memory_allign(uint32_t size, uint32_t alignment);
}