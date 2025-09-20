#pragma once

#include "PointerDefs.h"

namespace lcf::render {
    struct GPUResource : STDPointerDefs<GPUResource>
    {
        virtual ~GPUResource() {};
    };
}