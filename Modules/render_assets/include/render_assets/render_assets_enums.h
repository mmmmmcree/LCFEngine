#pragma once

#include "enums/enum_flags.h"
#include "enums/enum_values.h"
#include "enums/enum_name.h"
#include "vector_enums.h"
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
        return 1 << static_cast<uint16_t>(attribute);
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

namespace lcf::internal {
    enum class MaterialProperty : uint8_t
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

    inline constexpr uint32_t encode_flag_bit(MaterialProperty property) noexcept
    {
        return 1 << static_cast<uint32_t>(property);
    }

    inline constexpr uint16_t encode(MaterialProperty property, VectorType vector_type) noexcept
    {
        return (static_cast<uint16_t>(property) << 8) | std::to_underlying(vector_type);
    }
}

namespace lcf {
    enum class MaterialProperty : uint16_t
    {
        eBaseColor = internal::encode(internal::MaterialProperty::eBaseColor, VectorType::e4Float32),
        eRoughness = internal::encode(internal::MaterialProperty::eRoughness, VectorType::e1Float32),
        eMetallic = internal::encode(internal::MaterialProperty::eMetallic, VectorType::e1Float32),
        eReflectance = internal::encode(internal::MaterialProperty::eReflectance, VectorType::e1Float32),
        eAmbientOcclusion = internal::encode(internal::MaterialProperty::eAmbientOcclusion, VectorType::e1Float32),
        eClearCoat = internal::encode(internal::MaterialProperty::eClearCoat, VectorType::e1Float32),
        eClearCoatRoughness = internal::encode(internal::MaterialProperty::eClearCoatRoughness, VectorType::e1Float32),
        eClearCoatNormal = internal::encode(internal::MaterialProperty::eClearCoatNormal, VectorType::e1Float32),
        eAnisotropy = internal::encode(internal::MaterialProperty::eAnisotropy, VectorType::e1Float32),
        eAnisotropyDirection = internal::encode(internal::MaterialProperty::eAnisotropyDirection, VectorType::e3Float32),
        eThickness = internal::encode(internal::MaterialProperty::eThickness, VectorType::e1Float32),
        eSubsurfacePower = internal::encode(internal::MaterialProperty::eSubsurfacePower, VectorType::e1Float32),
        eSubsurfaceColor = internal::encode(internal::MaterialProperty::eSubsurfaceColor, VectorType::e3Float32),
        eSheenColor = internal::encode(internal::MaterialProperty::eSheenColor, VectorType::e3Float32),
        eSheenRoughness = internal::encode(internal::MaterialProperty::eSheenRoughness, VectorType::e3Float32),
        eEmissive = internal::encode(internal::MaterialProperty::eEmissive, VectorType::e4Float32),
        eNormal = internal::encode(internal::MaterialProperty::eNormal, VectorType::e3Float32),
        ePostLightingColor = internal::encode(internal::MaterialProperty::ePostLightingColor, VectorType::e4Float32),
        ePostLightingMixFactor = internal::encode(internal::MaterialProperty::ePostLightingMixFactor, VectorType::e1Float32),
        eAbsorption = internal::encode(internal::MaterialProperty::eAbsorption, VectorType::e3Float32),
        eTransmission = internal::encode(internal::MaterialProperty::eTransmission, VectorType::e1Float32), 
        eIOR = internal::encode(internal::MaterialProperty::eIOR, VectorType::e1Float32),
        eMicroThickness = internal::encode(internal::MaterialProperty::eMicroThickness, VectorType::e1Float32),
        eBentNormal = internal::encode(internal::MaterialProperty::eBentNormal, VectorType::e3Float32),
        eSpecularFactor = internal::encode(internal::MaterialProperty::eSpecularFactor, VectorType::e1Float32),
        eSpecularColorFactor = internal::encode(internal::MaterialProperty::eSpecularColorFactor, VectorType::e3Float32),
        eShadowStrength = internal::encode(internal::MaterialProperty::eShadowStrength, VectorType::e1Float32)
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

    enum class MaterialPropertyFlags : uint32_t
    {
        eBaseColor = internal::encode_flag_bit(internal::MaterialProperty::eBaseColor),
        eRoughness = internal::encode_flag_bit(internal::MaterialProperty::eRoughness),
        eMetallic = internal::encode_flag_bit(internal::MaterialProperty::eMetallic),
        eReflectance = internal::encode_flag_bit(internal::MaterialProperty::eReflectance),
        eAmbientOcclusion = internal::encode_flag_bit(internal::MaterialProperty::eAmbientOcclusion),
        eClearCoat = internal::encode_flag_bit(internal::MaterialProperty::eClearCoat),
        eClearCoatRoughness = internal::encode_flag_bit(internal::MaterialProperty::eClearCoatRoughness),
        eClearCoatNormal = internal::encode_flag_bit(internal::MaterialProperty::eClearCoatNormal),
        eAnisotropy = internal::encode_flag_bit(internal::MaterialProperty::eAnisotropy),
        eAnisotropyDirection = internal::encode_flag_bit(internal::MaterialProperty::eAnisotropyDirection),
        eThickness = internal::encode_flag_bit(internal::MaterialProperty::eThickness),
        eSubsurfacePower = internal::encode_flag_bit(internal::MaterialProperty::eSubsurfacePower),
        eSubsurfaceColor = internal::encode_flag_bit(internal::MaterialProperty::eSubsurfaceColor),
        eSheenColor = internal::encode_flag_bit(internal::MaterialProperty::eSheenColor),
        eSheenRoughness = internal::encode_flag_bit(internal::MaterialProperty::eSheenRoughness),
        eEmissive = internal::encode_flag_bit(internal::MaterialProperty::eEmissive),
        eNormal = internal::encode_flag_bit(internal::MaterialProperty::eNormal),
        ePostLightingColor = internal::encode_flag_bit(internal::MaterialProperty::ePostLightingColor),
        ePostLightingMixFactor = internal::encode_flag_bit(internal::MaterialProperty::ePostLightingMixFactor),
        eAbsorption = internal::encode_flag_bit(internal::MaterialProperty::eAbsorption),
        eTransmission = internal::encode_flag_bit(internal::MaterialProperty::eTransmission),
        eIOR = internal::encode_flag_bit(internal::MaterialProperty::eIOR),
        eMicroThickness = internal::encode_flag_bit(internal::MaterialProperty::eMicroThickness),
        eBentNormal = internal::encode_flag_bit(internal::MaterialProperty::eBentNormal),
        eSpecularFactor = internal::encode_flag_bit(internal::MaterialProperty::eSpecularFactor),
        eSpecularColorFactor = internal::encode_flag_bit(internal::MaterialProperty::eSpecularColorFactor),
        eShadowStrength = internal::encode_flag_bit(internal::MaterialProperty::eShadowStrength),
    };
    template <> inline constexpr bool is_enum_flags_v<MaterialPropertyFlags> = true;

    enum class ShadingModel : uint32_t
    {
        eStandard = MaterialPropertyFlags::eBaseColor |
            MaterialPropertyFlags::eRoughness |
            MaterialPropertyFlags::eMetallic |
            MaterialPropertyFlags::eReflectance |
            MaterialPropertyFlags::eAmbientOcclusion |
            MaterialPropertyFlags::eEmissive |
            MaterialPropertyFlags::eNormal, 
        eClearCoat = static_cast<MaterialPropertyFlags>(eStandard) |
            MaterialPropertyFlags::eClearCoat |
            MaterialPropertyFlags::eClearCoatRoughness |
            MaterialPropertyFlags::eClearCoatNormal, //- layout: 4 | 1, 1, 1, 1 | 4 | 3 , 1 | 1, 1 ; 20 bytes
        eSheen = static_cast<MaterialPropertyFlags>(eStandard) |
            MaterialPropertyFlags::eSheenColor |
            MaterialPropertyFlags::eSheenRoughness,
        eTransmission = static_cast<MaterialPropertyFlags>(eStandard) |
            MaterialPropertyFlags::eTransmission |
            MaterialPropertyFlags::eIOR |
            MaterialPropertyFlags::eThickness |
            MaterialPropertyFlags::eAbsorption,
        eAnisotropic = static_cast<MaterialPropertyFlags>(eStandard) |
            MaterialPropertyFlags::eAnisotropy |
            MaterialPropertyFlags::eAnisotropyDirection,
        eClearCoatAnisotropic = static_cast<MaterialPropertyFlags>(eClearCoat) |
            static_cast<MaterialPropertyFlags>(eAnisotropic),
        eUnlit = MaterialPropertyFlags::eBaseColor |
            MaterialPropertyFlags::eEmissive |
            MaterialPropertyFlags::ePostLightingColor,
        eSubsurface = MaterialPropertyFlags::eThickness |
            MaterialPropertyFlags::eSubsurfaceColor |
            MaterialPropertyFlags::eSubsurfacePower, 
        eSubsurfaceTransmission = static_cast<MaterialPropertyFlags>(eSubsurface) |
            MaterialPropertyFlags::eTransmission |
            MaterialPropertyFlags::eIOR,
        eCloth = MaterialPropertyFlags::eSheenColor |
            MaterialPropertyFlags::eSubsurfaceColor,
    };
}

namespace lcf::enum_decode {
    inline constexpr MaterialPropertyFlags to_flag_bit(MaterialProperty property) noexcept
    {
        return static_cast<MaterialPropertyFlags>(1 << (std::to_underlying(property) >> 8));
    }

    inline constexpr VectorType get_vector_type(MaterialProperty property) noexcept
    {
        return static_cast<VectorType>(std::to_underlying(property) & 0x3F);
    }

    inline constexpr MaterialPropertyFlags get_material_property_flags(ShadingModel shading_model) noexcept
    {
        return static_cast<MaterialPropertyFlags>(shading_model);
    }
}

namespace lcf {
    enum class TextureSemantic : uint8_t
    {
        eBaseColor,
        eMetallicRoughness,
        eNormal,
        eOcclusion,
        eEmissive,
        eHeight,
        eClearCoat,
        eClearCoatRoughness,
        eClearCoatNormal,
        eSheenColor,
        eSheenRoughness,
        eVolumeThickness,
        eTransmission,
        eSpecularColor,
        eSpecular
    };
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