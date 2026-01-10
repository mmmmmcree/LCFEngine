#pragma once

#include "enum_flags.h"
#include "enum_cast.h"
#include <limits>

namespace lcf {
    enum class VertexSemantic : uint8_t
    {
        ePosition,
        eNormal,
        eTexCoord0,
        eTexCoord1,
        eColor0,
        eColor1,
        eTangent,
        eBinormal,
        eJoints,
        eWeights
    };

    enum class VertexSemanticFlags : uint16_t
    {
        eNone = 0,
        ePosition = 1 << 0,
        eNormal = 1 << 1,
        eTexCoord0 = 1 << 2,
        eTexCoord1 = 1 << 3,
        eColor0 = 1 << 4,
        eColor1 = 1 << 5,
        eTangent = 1 << 6,
        eBinormal = 1 << 7,
        eJoints = 1 << 8,
        eWeights = 1 << 9,
        eAll = ePosition | eNormal | eTexCoord0 | eTexCoord1 | eColor0 | eColor1 | eTangent | eBinormal | eJoints | eWeights,
    };
    MAKE_ENUM_FLAGS(VertexSemanticFlags);

    enum class MaterialType : uint16_t
    {
        eCustom = 0,            // User-defined
        // === Legacy Shading Models (Pre-PBR) ===
        eBlinnPhong = 1 << 0,              // Blinn-Phong (most common legacy)
        ePhong = 1 << 1,                   // Classic Phong (older, rarely used now)
        eLambert = 1 << 2,                 // Pure diffuse (no specular)
        // === PBR Metallic Workflows ===
        ePBR_MetallicRoughness = 1 << 3,   // glTF standard (most common)
        // === PBR Specular Workflows ===
        ePBR_SpecularGlossiness = 1 << 4,  // Legacy PBR (older glTF extension)
        // === Extended PBR (Optional, for advanced materials) ===
        ePBR_ClearCoat = 1 << 5,           // PBR + clearcoat layer (car paint, lacquer)
        ePBR_Cloth = 1 << 5,               // Fabric/textile with sheen
        ePBR_Subsurface = 1 << 6,          // Skin, wax, jade (SSS scattering)
        ePBR_Transmission = 1 << 7,        // Glass, water (refraction)
        // === Special Types ===
        eUnlit = 1 << 8,                   // No lighting calculation (UI, skybox)
        // === Combinations ===
        eLegacyShadingModels = eBlinnPhong | ePhong | eLambert,
        ePBR = ePBR_MetallicRoughness | ePBR_SpecularGlossiness,
        eShadingModels = eLegacyShadingModels | ePBR,
        eExtendedPBR = ePBR_ClearCoat | ePBR_Cloth | ePBR_Subsurface | ePBR_Transmission,
        eAll = eCustom | eLegacyShadingModels | ePBR | eExtendedPBR | eUnlit,
    };

    enum class TextureSemantic : uint32_t // 16 bits for MaterialType, 16 bits for slot index,
    {
        eBaseColor = (std::to_underlying(MaterialType::ePBR) << 16) | 0,
        eDiffuse = (std::to_underlying(MaterialType::eLegacyShadingModels) << 16) | 0,
        eNormal = (std::to_underlying(MaterialType::eShadingModels) << 16) | 1,
        eMetallicRoughness = (std::to_underlying(MaterialType::ePBR_MetallicRoughness) << 16) | 2,
        eSpecularGlossiness = (std::to_underlying(MaterialType::ePBR_SpecularGlossiness) << 16) | 2,
        eSpecular = (std::to_underlying(MaterialType::eLegacyShadingModels) << 16) | 2,
        eEmissive = (std::to_underlying(MaterialType::eShadingModels) << 16) | 3,
        eAmbientOcclusion = (std::to_underlying(MaterialType::eShadingModels) << 16) | 4,
        eHeight = (std::to_underlying(MaterialType::eShadingModels) << 16) | 5,
        eOpacity = (std::to_underlying(MaterialType::eShadingModels) << 16) | 6,

        eCustomSlot0 = 0,
        eCustomSlot1 = 1,
        eCustomSlot2 = 2,
        eCustomSlot3 = 3,
        eCustomSlot4 = 4,
        eCustomSlot5 = 5,
        eCustomSlot6 = 6,
        eCustomSlot7 = 7,
        eCustomSlot8 = 8,
        eCustomSlot9 = 9,
        eCustomSlot10 = 10,
        eCustomSlot11 = 11,
        eCustomSlot12 = 12,
        eCustomSlot13 = 13,
        eCustomSlot14 = 14,
        eCustomSlot15 = 15,
    };
}
