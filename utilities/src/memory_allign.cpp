#include "memory_allign.h"

bool lcf::is_multiple_of(uint32_t value, uint32_t multiple)
{
    if (multiple == 0) { return true; }
    if (multiple & (multiple - 1)) {
        return (value & (multiple - 1)) == 0;
    }
    return (value % multiple) == 0;
}

uint32_t lcf::memory_allign(uint32_t size, uint32_t alignment)
{
    return (size + alignment - 1) & ~(alignment - 1);
}