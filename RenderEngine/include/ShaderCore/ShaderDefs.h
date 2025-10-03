#pragma once

#include <shaderc/shaderc.hpp>
#include <spirv_cross/spirv_cross.hpp>
#include <vulkan/vulkan.hpp>
#include "enum_cast.h"
#include "enum_flags.h"
#include <vector>

namespace lcf {
    using SpvCode = std::vector<uint32_t>;

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
    MAKE_ENUM_FLAGS(ShaderTypeFlagBits);

    struct EnumShaderTypeTag {};

    template <> struct enum_category<ShaderTypeFlagBits, vk::ShaderStageFlagBits> { using type = EnumShaderTypeTag; };
    template <> struct enum_category<ShaderTypeFlagBits, shaderc_shader_kind> { using type = EnumShaderTypeTag; };

    template <>
    struct enum_mapping_traits<EnumShaderTypeTag>
    {
        using mapping_type = std::tuple<ShaderTypeFlagBits, vk::ShaderStageFlagBits, shaderc_shader_kind>;
        constexpr static mapping_type mappings[] = {
            { ShaderTypeFlagBits::eVertex, vk::ShaderStageFlagBits::eVertex, shaderc_vertex_shader },
            { ShaderTypeFlagBits::eTessControl, vk::ShaderStageFlagBits::eTessellationControl, shaderc_tess_control_shader },
            { ShaderTypeFlagBits::eTessEvaluation, vk::ShaderStageFlagBits::eTessellationEvaluation, shaderc_tess_evaluation_shader },
            { ShaderTypeFlagBits::eGeometry, vk::ShaderStageFlagBits::eGeometry, shaderc_geometry_shader },
            { ShaderTypeFlagBits::eFragment, vk::ShaderStageFlagBits::eFragment, shaderc_fragment_shader },
            { ShaderTypeFlagBits::eCompute, vk::ShaderStageFlagBits::eCompute, shaderc_compute_shader },
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

    struct EnumShaderBaseDataTypeTag {};

    template <> struct enum_category<ShaderDataType, spirv_cross::SPIRType::BaseType> { using type = EnumShaderBaseDataTypeTag; };

    template <>
    struct enum_mapping_traits<EnumShaderBaseDataTypeTag>
    {
        using mapping_type = std::tuple<ShaderDataType, spirv_cross::SPIRType::BaseType>;
        constexpr static mapping_type mappings[] = {
            { ShaderDataType::Unknown, spirv_cross::SPIRType::Unknown},
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
            { ShaderDataType::Char, spirv_cross::SPIRType::Char },
        };
    };

    struct EnumShaderDataTypeTag {};

    template <> struct enum_category<ShaderDataType, vk::Format> { using type = EnumShaderDataTypeTag; };

    template <>
    struct enum_mapping_traits<EnumShaderDataTypeTag>
    {
        using mapping_type = std::tuple<ShaderDataType, vk::Format>;
        constexpr static mapping_type mappings[] = {
            { ShaderDataType::Unknown, vk::Format::eUndefined },
            { ShaderDataType::Void, vk::Format::eUndefined },
            { ShaderDataType::Boolean, vk::Format::eR8Unorm },
            { ShaderDataType::BooleanVec2, vk::Format::eR8G8Unorm },
            { ShaderDataType::BooleanVec3, vk::Format::eR8G8B8Unorm },
            { ShaderDataType::BooleanVec4, vk::Format::eR8G8B8A8Unorm },
            { ShaderDataType::SByte, vk::Format::eR8Snorm },
            { ShaderDataType::SByteVec2, vk::Format::eR8G8Snorm },
            { ShaderDataType::SByteVec3, vk::Format::eR8G8B8Snorm },
            { ShaderDataType::SByteVec4, vk::Format::eR8G8B8A8Snorm },
            { ShaderDataType::UByte, vk::Format::eR8Uint },
            { ShaderDataType::UByteVec2, vk::Format::eR8G8Uint },
            { ShaderDataType::UByteVec3, vk::Format::eR8G8B8Uint },
            { ShaderDataType::UByteVec4, vk::Format::eR8G8B8A8Uint },
            { ShaderDataType::Short, vk::Format::eR16Snorm },
            { ShaderDataType::ShortVec2, vk::Format::eR16G16Snorm },
            { ShaderDataType::ShortVec3, vk::Format::eR16G16B16Snorm },
            { ShaderDataType::ShortVec4, vk::Format::eR16G16B16A16Snorm },
            { ShaderDataType::UShort, vk::Format::eR16Uint },
            { ShaderDataType::UShortVec2, vk::Format::eR16G16Uint },
            { ShaderDataType::UShortVec3, vk::Format::eR16G16B16Uint },
            { ShaderDataType::UShortVec4, vk::Format::eR16G16B16A16Uint },
            { ShaderDataType::Int, vk::Format::eR32Sint },
            { ShaderDataType::IntVec2, vk::Format::eR32G32Sint },
            { ShaderDataType::IntVec3, vk::Format::eR32G32B32Sint },
            { ShaderDataType::IntVec4, vk::Format::eR32G32B32A32Sint },
            { ShaderDataType::UInt, vk::Format::eR32Uint },
            { ShaderDataType::UIntVec2, vk::Format::eR32G32Uint },
            { ShaderDataType::UIntVec3, vk::Format::eR32G32B32Uint },
            { ShaderDataType::UIntVec4, vk::Format::eR32G32B32A32Uint },
            { ShaderDataType::Int64, vk::Format::eR64Sint },
            { ShaderDataType::Int64Vec2, vk::Format::eR64G64Sint },
            { ShaderDataType::Int64Vec3, vk::Format::eR64G64B64Sint },
            { ShaderDataType::Int64Vec4, vk::Format::eR64G64B64A64Sint },
            { ShaderDataType::UInt64, vk::Format::eR64Uint },
            { ShaderDataType::UInt64Vec2, vk::Format::eR64G64Uint },
            { ShaderDataType::UInt64Vec3, vk::Format::eR64G64B64Uint },
            { ShaderDataType::UInt64Vec4, vk::Format::eR64G64B64A64Uint },
            { ShaderDataType::Half, vk::Format::eR16Sfloat },
            { ShaderDataType::HalfVec2, vk::Format::eR16G16Sfloat },
            { ShaderDataType::HalfVec3, vk::Format::eR16G16B16Sfloat },
            { ShaderDataType::HalfVec4, vk::Format::eR16G16B16A16Sfloat },
            { ShaderDataType::Float, vk::Format::eR32Sfloat },
            { ShaderDataType::FloatVec2, vk::Format::eR32G32Sfloat },
            { ShaderDataType::FloatVec3, vk::Format::eR32G32B32Sfloat },
            { ShaderDataType::FloatVec4, vk::Format::eR32G32B32A32Sfloat },
            { ShaderDataType::Double, vk::Format::eR64Sfloat },
            { ShaderDataType::DoubleVec2, vk::Format::eR64G64Sfloat },
            { ShaderDataType::DoubleVec3, vk::Format::eR64G64B64Sfloat },
            { ShaderDataType::DoubleVec4, vk::Format::eR64G64B64A64Sfloat },
            { ShaderDataType::Char, vk::Format::eUndefined },
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