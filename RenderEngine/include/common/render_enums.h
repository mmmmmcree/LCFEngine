#pragma once

#include <stdint.h>

namespace lcf::render {
    enum class GPUBufferPattern : uint8_t
    {
        eDynamic, // frequently update
        eStatic,  // rarely update
    };

    enum class GPUBufferUsage : uint8_t
    {
        eUndefined = 0,
        eVertex,
        eIndex,
        eUniform,
        eShaderStorage,
        eIndirect,
        eStaging,
    };

    enum class DescriptorSetBindingPoints : uint8_t
    {
        ePerView = 0,
        ePerRenderable = 1,
        ePerMaterial = 2,
    };

    enum class PerViewBindingPoints : uint8_t
    {
        eCamera = 0,
    };

    enum class PerRenderableBindingPoints : uint8_t
    {
        eVertexBuffer = 0,
        eIndexBuffer = 1,
        eTransform = 2,
    };

    enum class PerMaterialBindingPoints : uint8_t
    {
        eProperties = 0,
    };

    enum class SamplerPreset : uint8_t
    {
        // Material 
        eColorMap,
        eNormalMap,
        eMaterialMap,
        eEmissiveMap,
        // Environment
        eEnvironmentMap,
        eIrradianceMap,
        ePrefilteredMap,
        eBRDFLUT,
        // Special Effect
        eUITexture,
        eSpriteTexture,
        eShadowMap,
        ePointSampled,
        // Post Process
        eScreenTexture,
        eBlurTexture,
    };
}