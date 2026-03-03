#pragma once

#include <stdint.h>

namespace lcf {
    enum class TaskSchedulerRunMode : uint8_t
    {
        eThisThread,
        eNewThread
    };
}