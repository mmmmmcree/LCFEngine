#pragma once

#include <stdint.h>

namespace lcf {
    enum class ResourceState : uint8_t
    {
        eUnloaded,
        eLoading,
        eLoaded,
        eInUse,
        eUnloading,
        eReleased
    };
}