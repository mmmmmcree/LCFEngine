#pragma once

#include "PointerDefs.h"

namespace lcf::render {
    struct GPUResource : PointerDefs<GPUResource>
    {
        virtual ~GPUResource() {};
    };
}