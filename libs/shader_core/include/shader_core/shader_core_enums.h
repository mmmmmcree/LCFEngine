#pragma once

#include <shaderc/shaderc.hpp>
#include <spirv_cross/spirv_common.hpp>
#include "enums/enum_cast.h"
#include "enums/enum_flags.h"

namespace lcf {
    enum class ShaderTypeFlagBits : uint8_t 
    {
        eVertex = 1,
        eTessControl = 1 << 1,
        eTessEvaluation = 1 << 2,
        eGeometry = 1 << 3,
        eFragment = 1 << 4,
        eCompute = 1 << 5,
        eAllGraphics = eVertex | eTessControl | eTessEvaluation | eGeometry | eFragment,
        eAll = eAllGraphics | eCompute,
    };
    template <> inline constexpr bool is_enum_flags_v<ShaderTypeFlagBits> = true;

    template <>
    struct enum_mapping_traits <ShaderTypeFlagBits, shaderc_shader_kind>
    {
        static constexpr std::tuple<ShaderTypeFlagBits, shaderc_shader_kind> mappings[] = {
            { ShaderTypeFlagBits::eVertex, shaderc_vertex_shader },
            { ShaderTypeFlagBits::eTessControl, shaderc_tess_control_shader },
            { ShaderTypeFlagBits::eTessEvaluation, shaderc_tess_evaluation_shader },
            { ShaderTypeFlagBits::eGeometry, shaderc_geometry_shader },
            { ShaderTypeFlagBits::eFragment, shaderc_fragment_shader },
            { ShaderTypeFlagBits::eCompute, shaderc_compute_shader },
        };
    };

    enum class ShaderDataType
    {
        Unknown,
		Void,
		Boolean,
        BooleanVec2,
        BooleanVec3,
        BooleanVec4,
		SByte,
        SByteVec2,
        SByteVec3,
        SByteVec4,
		UByte,
        UByteVec2,
        UByteVec3,
        UByteVec4,
		Short,
        ShortVec2,
        ShortVec3,
        ShortVec4,
		UShort,
        UShortVec2,
        UShortVec3,
        UShortVec4,
		Int,
        IntVec2,
        IntVec3,
        IntVec4,
		UInt,
        UIntVec2,
        UIntVec3,
        UIntVec4,
		Int64,
        Int64Vec2,
        Int64Vec3,
        Int64Vec4,
		UInt64,
        UInt64Vec2,
        UInt64Vec3,
        UInt64Vec4,
		AtomicCounter,
		Half,
        HalfVec2,
        HalfVec3,
        HalfVec4,
		Float,
        FloatVec2,
        FloatVec3,
        FloatVec4,
		Double,
        DoubleVec2,
        DoubleVec3,
        DoubleVec4,
		Struct,
		Image,
		SampledImage,
		Sampler,
		AccelerationStructure,
		RayQuery,
		ControlPointArray,
		Interpolant,
		Char
    };

    template <>
    struct enum_mapping_traits <ShaderDataType, spirv_cross::SPIRType::BaseType>
    {
        static constexpr std::tuple<ShaderDataType, spirv_cross::SPIRType::BaseType> mappings[] = {
            { ShaderDataType::Unknown, spirv_cross::SPIRType::Unknown },
            { ShaderDataType::Void, spirv_cross::SPIRType::Void },
            { ShaderDataType::Boolean, spirv_cross::SPIRType::Boolean },
            { ShaderDataType::SByte, spirv_cross::SPIRType::SByte },
            { ShaderDataType::UByte, spirv_cross::SPIRType::UByte },
            { ShaderDataType::Short, spirv_cross::SPIRType::Short },
            { ShaderDataType::UShort, spirv_cross::SPIRType::UShort },
            { ShaderDataType::Int, spirv_cross::SPIRType::Int },
            { ShaderDataType::UInt, spirv_cross::SPIRType::UInt },
            { ShaderDataType::Int64, spirv_cross::SPIRType::Int64 },
            { ShaderDataType::UInt64, spirv_cross::SPIRType::UInt64 },
            { ShaderDataType::AtomicCounter, spirv_cross::SPIRType::AtomicCounter },
            { ShaderDataType::Half, spirv_cross::SPIRType::Half },
            { ShaderDataType::Float, spirv_cross::SPIRType::Float },
            { ShaderDataType::Double, spirv_cross::SPIRType::Double },
            { ShaderDataType::Struct, spirv_cross::SPIRType::Struct },
            { ShaderDataType::Image, spirv_cross::SPIRType::Image },
            { ShaderDataType::SampledImage, spirv_cross::SPIRType::SampledImage },
            { ShaderDataType::Sampler, spirv_cross::SPIRType::Sampler },
            { ShaderDataType::AccelerationStructure, spirv_cross::SPIRType::AccelerationStructure },
            { ShaderDataType::RayQuery, spirv_cross::SPIRType::RayQuery },
            { ShaderDataType::ControlPointArray, spirv_cross::SPIRType::ControlPointArray },
            { ShaderDataType::Interpolant, spirv_cross::SPIRType::Interpolant },
            { ShaderDataType::Char, spirv_cross::SPIRType::Char }
        };
    };

    enum class ShaderResourceType
    {
        Sampler,
        CombinedImageSampler,
        SampledImage,
        StorageImage,
        UniformTexelBuffer,
        StorageTexelBuffer,
        UniformBuffer,
        StorageBuffer,
        PushConstant,
    };
}