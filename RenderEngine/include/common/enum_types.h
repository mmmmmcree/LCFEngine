#pragma once

#include <stdint.h>

namespace lcf::render {
    enum class GPUBufferPattern
    {
        eDynamic, // frequently update
        eStatic,  // rarely update
    };

    enum class GPUBufferUsage : uint8_t {
        eUndefined = 0,
        eVertex,
        eIndex,
        eUniform,
        eShaderStorage,
        eStaging,
    };

    enum class DescriptorSetBindingPoints : uint8_t {
        ePerView = 0,
        ePerRenderable = 1,
        ePerMaterial = 2,
    };

    enum class PerViewBindingPoints : uint8_t {
        eCamera = 0,
    };

    enum class PerRenderableBindingPoints : uint8_t {
        eTransform = 0,
    };

    enum class PerMaterialBindingPoints : uint8_t {
        eProperties = 0,
    };
}