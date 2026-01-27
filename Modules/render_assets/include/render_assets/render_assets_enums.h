#pragma once

#include "enums/enum_flags.h"
#include "enums/enum_values.h"
#include "enums/enum_name.h"
#include "math_enums.h"
#include <utility>
#include <limits>
#include <array>

namespace lcf::internal {
    enum class VertexAttribute : uint8_t // 4 bits used
    {
        ePosition,
        eNormal,
        eTexCoord0,
        eTexCoord1,
        eColor,
        eTangent,
        ePackedTBN,
        eJoints,
        eWeights,
        eCustom0,
        eCustom1,
        eCustom2,
        eCustom3,
        eCustom4,
        eCustom5,
        eCustom6,
    };

    inline constexpr uint16_t encode_flag_bit(VertexAttribute attribute) noexcept
    {
        return 1 << std::to_underlying(attribute);
    }

    inline constexpr uint16_t encode(VertexAttribute attribute, VectorType type) noexcept
    {
        return std::to_underlying(attribute) << 12 | std::to_underlying(type);
    }
}

namespace lcf {
    enum class VertexAttribute : uint16_t // 4 bits for VertexAttribute | 6 bits for VectorType, 10 bits used
    {
        ePosition = internal::encode(internal::VertexAttribute::ePosition, VectorType::e3Float32), //!< XYZ position (float3)
        eNormal = internal::encode(internal::VertexAttribute::eNormal, VectorType::e3Float32), //!< Normal vector (float3)
        eTexCoord0 = internal::encode(internal::VertexAttribute::eTexCoord0, VectorType::e2Float32), //!< texture coordinates (float2)
        eTexCoord1 = internal::encode(internal::VertexAttribute::eTexCoord1, VectorType::e2Float32), //!< texture coordinates (float2)
        eColor = internal::encode(internal::VertexAttribute::eColor, VectorType::e4Float32), //!< vertex color (float4)
        eTangent = internal::encode(internal::VertexAttribute::eTangent, VectorType::e3Float32), //!< tangent vector (float3)
        ePackedTBN = internal::encode(internal::VertexAttribute::ePackedTBN, VectorType::e4Float32), //!< tangent, bitangent and normal, encoded as a quaternion (float4)
        eJoints = internal::encode(internal::VertexAttribute::eJoints, VectorType::e4UInt32), //!< indices of 4 bones, as unsigned integers (uvec4)
        eWeights = internal::encode(internal::VertexAttribute::eWeights, VectorType::e4Float32), //!< weights of the 4 bones (normalized float4)
        eCustom0 = internal::encode(internal::VertexAttribute::eCustom0, VectorType::e4Float32), //!< custom attribute (float4)
        eCustom1 = internal::encode(internal::VertexAttribute::eCustom1, VectorType::e4Float32),
        eCustom2 = internal::encode(internal::VertexAttribute::eCustom2, VectorType::e4Float32),
        eCustom3 = internal::encode(internal::VertexAttribute::eCustom3, VectorType::e4Float32), 
        eCustom4 = internal::encode(internal::VertexAttribute::eCustom4, VectorType::e4Float32), 
        eCustom5 = internal::encode(internal::VertexAttribute::eCustom5, VectorType::e4Float32), 
        eCustom6 = internal::encode(internal::VertexAttribute::eCustom6, VectorType::e4Float32), 
    };

    template <> inline constexpr std::array<VertexAttribute, enum_count_v<internal::VertexAttribute>> enum_values_v<VertexAttribute>
    {
        VertexAttribute::ePosition,
        VertexAttribute::eNormal,
        VertexAttribute::eTexCoord0,
        VertexAttribute::eTexCoord1,
        VertexAttribute::eColor,
        VertexAttribute::eTangent,
        VertexAttribute::ePackedTBN,
        VertexAttribute::eJoints,
        VertexAttribute::eWeights,
        VertexAttribute::eCustom0,
        VertexAttribute::eCustom1,
        VertexAttribute::eCustom2,
        VertexAttribute::eCustom3,
        VertexAttribute::eCustom4,
        VertexAttribute::eCustom5,
        VertexAttribute::eCustom6,
    };

    enum class VertexAttributeFlags : uint16_t
    {
        eNone = 0,
        ePosition = internal::encode_flag_bit(internal::VertexAttribute::ePosition),
        eNormal = internal::encode_flag_bit(internal::VertexAttribute::eNormal),
        eTexCoord0 = internal::encode_flag_bit(internal::VertexAttribute::eTexCoord0),
        eTexCoord1 = internal::encode_flag_bit(internal::VertexAttribute::eTexCoord1),
        eColor = internal::encode_flag_bit(internal::VertexAttribute::eColor),
        eTangent = internal::encode_flag_bit(internal::VertexAttribute::eTangent),
        ePackedTBN = internal::encode_flag_bit(internal::VertexAttribute::ePackedTBN),
        eJoints = internal::encode_flag_bit(internal::VertexAttribute::eJoints),
        eWeights = internal::encode_flag_bit(internal::VertexAttribute::eWeights),
        eCustom0 = internal::encode_flag_bit(internal::VertexAttribute::eCustom0),
        eCustom1 = internal::encode_flag_bit(internal::VertexAttribute::eCustom1),
        eCustom2 = internal::encode_flag_bit(internal::VertexAttribute::eCustom2),
        eCustom3 = internal::encode_flag_bit(internal::VertexAttribute::eCustom3),
        eCustom4 = internal::encode_flag_bit(internal::VertexAttribute::eCustom4),
        eCustom5 = internal::encode_flag_bit(internal::VertexAttribute::eCustom5),
        eCustom6 = internal::encode_flag_bit(internal::VertexAttribute::eCustom6),
        eAll = std::numeric_limits<uint16_t>::max()
    };
    template <> inline constexpr bool is_enum_flags_v<VertexAttributeFlags> = true;
}

namespace lcf::enum_decode {
    inline constexpr VertexAttributeFlags to_flag_bit(VertexAttribute attribute) noexcept
    {
        return static_cast<VertexAttributeFlags>(internal::encode_flag_bit(static_cast<internal::VertexAttribute>(std::to_underlying(attribute) >> 12)));
    }

    inline constexpr VectorType get_vector_type(VertexAttribute attribute) noexcept
    {
        return static_cast<VectorType>(std::to_underlying(attribute) & 0x3F);
    }
    
    inline constexpr BasicDataType get_basic_data_type(VertexAttribute attribute) noexcept
    {
        return get_basic_data_type(get_vector_type(attribute));
    }

    inline constexpr uint32_t get_vector_dimension(VertexAttribute attribute) noexcept
    {
        return get_vector_dimension(get_vector_type(attribute));
    }

    inline constexpr uint32_t get_index(VertexAttribute attribute) noexcept
    {
        return std::to_underlying(attribute) >> 12;
    }
}

namespace lcf {
    enum class ShadingModel : uint8_t // 4 bits used, 4 bits reserved, 8 bits used
    {
        eUnlit = 1 << 0, //!< no lighting applied, emissive possible
        eLit = 1 << 1, //!< default, standard lighting
        eSubsurface = 1 << 2, //!< subsurface lighting model
        eCloth = 1 << 3, //!< cloth lighting model
        eAll = eUnlit | eLit | eSubsurface | eCloth
    };
    template <> inline constexpr bool is_enum_flags_v<ShadingModel> = true;
}

namespace lcf::internal {
    enum class MaterialProperty : uint8_t // 5 bits used
    {
        eBaseColor,
        eRoughness,
        eMetallic,
        eReflectance,
        eAmbientOcclusion,
        eClearCoat,
        eClearCoatRoughness,
        eClearCoatNormal,
        eAnisotropy,
        eAnisotropyDirection,
        eThickness,
        eSubsurfacePower,
        eSubsurfaceColor,
        eSheenColor,
        eSheenRoughness,
        eEmissive,
        eNormal,
        ePostLightingColor,
        ePostLightingMixFactor,
        eAbsorption,
        eTransmission,
        eIOR,
        eMicroThickness,
        eBentNormal,
        eSpecularFactor,
        eSpecularColorFactor,
        eShadowStrength,
    };

    inline constexpr uint32_t encode(MaterialProperty property, ShadingModel shading_model, VectorType type) noexcept
    {
        return static_cast<uint32_t>(property) << 24 | static_cast<uint32_t>(shading_model) << 8 | std::to_underlying(type);
    }
}

namespace lcf {
    enum class MaterialProperty : uint32_t // 5 bits for MaterialProperty | 8 bits for ShadingModel | 6 bits for VectorType, 19 bits used
    { 
        eBaseColor = internal::encode(internal::MaterialProperty::eBaseColor, ShadingModel::eAll, VectorType::e4Float32), //!< float4, all shading models
        eRoughness = internal::encode(internal::MaterialProperty::eRoughness, ShadingModel::eLit, VectorType::e1Float32), //!< float,  lit shading models only
        eMetallic = internal::encode(internal::MaterialProperty::eMetallic, ShadingModel::eAll &~ ShadingModel::eUnlit &~ ShadingModel::eCloth, VectorType::e1Float32), //!< float,  all shading models, except unlit and cloth
        eReflectance = internal::encode(internal::MaterialProperty::eReflectance, ShadingModel::eAll &~ ShadingModel::eUnlit &~ ShadingModel::eCloth, VectorType::e1Float32), //!< float,  all shading models, except unlit and cloth
        eAmbientOcclusion = internal::encode(internal::MaterialProperty::eAmbientOcclusion, ShadingModel::eLit, VectorType::e1Float32), //!< float,  lit shading models only, except subsurface and cloth
        eClearCoat = internal::encode(internal::MaterialProperty::eClearCoat, ShadingModel::eLit, VectorType::e1Float32), //!< float,  lit shading models only, except subsurface and cloth
        eClearCoatRoughness = internal::encode(internal::MaterialProperty::eClearCoatRoughness, ShadingModel::eLit, VectorType::e1Float32), //!< float,  lit shading models only, except subsurface and cloth
        eClearCoatNormal = internal::encode(internal::MaterialProperty::eClearCoatNormal, ShadingModel::eLit, VectorType::e1Float32), //!< float,  lit shading models only, except subsurface and cloth
        eAnisotropy = internal::encode(internal::MaterialProperty::eAnisotropy, ShadingModel::eLit, VectorType::e1Float32), //!< float,  lit shading models only, except subsurface and cloth
        eAnisotropyDirection = internal::encode(internal::MaterialProperty::eAnisotropyDirection, ShadingModel::eLit, VectorType::e3Float32), //!< float3, lit shading models only, except subsurface and cloth
        eThickness = internal::encode(internal::MaterialProperty::eThickness, ShadingModel::eSubsurface, VectorType::e1Float32), //!< float,  subsurface shading model only
        eSubsurfacePower = internal::encode(internal::MaterialProperty::eSubsurfacePower, ShadingModel::eSubsurface, VectorType::e1Float32), //!< float,  subsurface shading model only
        eSubsurfaceColor = internal::encode(internal::MaterialProperty::eSubsurfaceColor, ShadingModel::eSubsurface, VectorType::e3Float32), //!< float3, subsurface and cloth shading models only
        eSheenColor = internal::encode(internal::MaterialProperty::eSheenColor, ShadingModel::eLit, VectorType::e3Float32), //!< float3, lit shading models only, except subsurface
        eSheenRoughness = internal::encode(internal::MaterialProperty::eSheenRoughness, ShadingModel::eLit, VectorType::e3Float32), //!< float3, lit shading models only, except subsurface and cloth
        eEmissive = internal::encode(internal::MaterialProperty::eEmissive, ShadingModel::eAll, VectorType::e4Float32), //!< float4, all shading models
        eNormal = internal::encode(internal::MaterialProperty::eNormal, ShadingModel::eAll &~ ShadingModel::eUnlit, VectorType::e3Float32), //!< float3, all shading models only, except unlit
        ePostLightingColor = internal::encode(internal::MaterialProperty::ePostLightingColor, ShadingModel::eAll, VectorType::e4Float32), //!< float4, all shading models
        ePostLightingMixFactor = internal::encode(internal::MaterialProperty::ePostLightingMixFactor, ShadingModel::eAll, VectorType::e1Float32), //!< float, all shading models
        eAbsorption = internal::encode(internal::MaterialProperty::eAbsorption, ShadingModel::eLit, VectorType::e3Float32), //!< float3, how much light is absorbed by the material
        eTransmission = internal::encode(internal::MaterialProperty::eTransmission, ShadingModel::eLit, VectorType::e1Float32), //!< float,  how much light is refracted through the material
        eIOR = internal::encode(internal::MaterialProperty::eIOR, ShadingModel::eLit, VectorType::e1Float32), //!< float,  material's index of refraction
        eMicroThickness = internal::encode(internal::MaterialProperty::eMicroThickness, ShadingModel::eLit, VectorType::e1Float32), //!< float, thickness of the thin layer
        eBentNormal = internal::encode(internal::MaterialProperty::eBentNormal, ShadingModel::eAll &~ ShadingModel::eUnlit, VectorType::e3Float32), //!< float3, all shading models only, except unlit
        eSpecularFactor = internal::encode(internal::MaterialProperty::eSpecularFactor, ShadingModel::eLit, VectorType::e1Float32), //!< float, lit shading models only, except subsurface and cloth
        eSpecularColorFactor = internal::encode(internal::MaterialProperty::eSpecularColorFactor, ShadingModel::eLit, VectorType::e3Float32), //!< float3, lit shading models only, except subsurface and cloth
        eShadowStrength = internal::encode(internal::MaterialProperty::eShadowStrength, ShadingModel::eLit, VectorType::e1Float32), //!< float, [0, 1] strength of shadows received by this material
    };


    template <> inline constexpr std::array<MaterialProperty, enum_count_v<internal::MaterialProperty>> enum_values_v<MaterialProperty>
    {
        MaterialProperty::eBaseColor,
        MaterialProperty::eRoughness,
        MaterialProperty::eMetallic,
        MaterialProperty::eReflectance,
        MaterialProperty::eAmbientOcclusion,
        MaterialProperty::eClearCoat,
        MaterialProperty::eClearCoatRoughness,
        MaterialProperty::eClearCoatNormal,
        MaterialProperty::eAnisotropy,
        MaterialProperty::eAnisotropyDirection,
        MaterialProperty::eThickness,
        MaterialProperty::eSubsurfacePower,
        MaterialProperty::eSubsurfaceColor,
        MaterialProperty::eSheenColor,
        MaterialProperty::eSheenRoughness,
        MaterialProperty::eEmissive,
        MaterialProperty::eNormal,
        MaterialProperty::ePostLightingColor,
        MaterialProperty::ePostLightingMixFactor,
        MaterialProperty::eAbsorption,
        MaterialProperty::eTransmission,
        MaterialProperty::eIOR,
        MaterialProperty::eMicroThickness,
        MaterialProperty::eBentNormal,
        MaterialProperty::eSpecularFactor,
        MaterialProperty::eSpecularColorFactor,
        MaterialProperty::eShadowStrength,
    };
}

namespace lcf::enum_decode {
    inline constexpr ShadingModel get_shading_model(MaterialProperty property) noexcept
    {
        return static_cast<ShadingModel>((std::to_underlying(property) >> 8) & 0xFF);
    }

    inline constexpr VectorType get_vector_type(MaterialProperty property) noexcept
    {
        return static_cast<VectorType>(std::to_underlying(property) & 0x3F);
    }
}

namespace lcf::internal {
    enum class TextureSemantic : uint8_t
    {
        eBaseColor,
        eDiffuse,
        eNormal,
        eMetallicRoughness,
        eSpecular,
        eEmissive,
        eAmbientOcclusion,
        eHeight,
        eOpacity,
        eCustomSlot0,
        eCustomSlot1,
        eCustomSlot2,
        eCustomSlot3,
        eCustomSlot4,
        eCustomSlot5,
        eCustomSlot6,
        eCustomSlot7,
        eCustomSlot8,
        eCustomSlot9,
        eCustomSlot10,
        eCustomSlot11,
        eCustomSlot12,
        eCustomSlot13,
        eCustomSlot14,
        eCustomSlot15,
    };

    inline constexpr uint16_t encode(TextureSemantic semantic, uint8_t slot_index) noexcept
    {
        return static_cast<uint16_t>(semantic) << 8 | slot_index;
    }
}

namespace lcf {
    enum class TextureSemantic : uint16_t // 8 bits for TextureSemantic | 8 bits for slot index, 16 bits used
    {
        eBaseColor = internal::encode(internal::TextureSemantic::eBaseColor, 0),
        eDiffuse = internal::encode(internal::TextureSemantic::eDiffuse, 1),
        eNormal = internal::encode(internal::TextureSemantic::eNormal, 2),
        eMetallicRoughness = internal::encode(internal::TextureSemantic::eMetallicRoughness, 3),
        eSpecular = internal::encode(internal::TextureSemantic::eSpecular, 4),
        eEmissive = internal::encode(internal::TextureSemantic::eEmissive, 5),
        eAmbientOcclusion = internal::encode(internal::TextureSemantic::eAmbientOcclusion, 6),
        eHeight = internal::encode(internal::TextureSemantic::eHeight, 7),
        eOpacity = internal::encode(internal::TextureSemantic::eOpacity, 8),
        eCustomSlot0 = internal::encode(internal::TextureSemantic::eCustomSlot0, 0),
        eCustomSlot1 = internal::encode(internal::TextureSemantic::eCustomSlot1, 1),
        eCustomSlot2 = internal::encode(internal::TextureSemantic::eCustomSlot2, 2),
        eCustomSlot3 = internal::encode(internal::TextureSemantic::eCustomSlot3, 3),
        eCustomSlot4 = internal::encode(internal::TextureSemantic::eCustomSlot4, 4),
        eCustomSlot5 = internal::encode(internal::TextureSemantic::eCustomSlot5, 5),
        eCustomSlot6 = internal::encode(internal::TextureSemantic::eCustomSlot6, 6),
        eCustomSlot7 = internal::encode(internal::TextureSemantic::eCustomSlot7, 7),
        eCustomSlot8 = internal::encode(internal::TextureSemantic::eCustomSlot8, 8),
        eCustomSlot9 = internal::encode(internal::TextureSemantic::eCustomSlot9, 9),
        eCustomSlot10 = internal::encode(internal::TextureSemantic::eCustomSlot10, 10),
        eCustomSlot11 = internal::encode(internal::TextureSemantic::eCustomSlot11, 11),
        eCustomSlot12 = internal::encode(internal::TextureSemantic::eCustomSlot12, 12),
        eCustomSlot13 = internal::encode(internal::TextureSemantic::eCustomSlot13, 13),
        eCustomSlot14 = internal::encode(internal::TextureSemantic::eCustomSlot14, 14),
        eCustomSlot15 = internal::encode(internal::TextureSemantic::eCustomSlot15, 15),
    };
}

namespace lcf::enum_decode {
    inline constexpr uint32_t get_slot_index(TextureSemantic semantic) noexcept
    {
        return std::to_underlying(semantic) & 0xFF;
    }
}

namespace lcf {
    enum class DefaultTexture2DType : uint8_t
    {
        eWhite,           //- #FFFFFFFF
        eBlack,           //- #000000FF
        eGray,            //- #808080FF 
        eRed,             //- #FF0000FF
        eGreen,           //- #00FF00FF
        eBlue,            //- #0000FFFF
        // for debug
        eCheckerboard,
    };
}