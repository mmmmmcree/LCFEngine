#pragma once

#include <shaderc/shaderc.hpp>
#include <spirv_cross/spirv_cross.hpp>
#include <vulkan/vulkan.hpp>
#include "enum_cast.h"
#include <vector>

namespace lcf {
    using SpvCode = std::vector<uint32_t>;

    enum class VertexInputLocation
    {
        Position = 0,
        Normal = 1,
        UV = 2,
    };

    constexpr const char * to_string(VertexInputLocation location)
    {
        const char *str = nullptr;
        switch (location) {
            case VertexInputLocation::Position: { str = "position"; } break;
            case VertexInputLocation::Normal: { str = "normal"; } break;
            case VertexInputLocation::UV: { str = "uv"; } break;
        }
        return str;
    }

    enum class ShaderTypeFlagBits
    {
        Vertex = 1,
        TessControl = 1 << 1,
        TessEvaluation = 1 << 2,
        Geometry = 1 << 3,
        Fragment = 1 << 4,
        Compute = 1 << 5,
        AllGraphics = Vertex | TessControl | TessEvaluation | Geometry | Fragment,
        All = AllGraphics | Compute,
    };

    struct EnumShaderTypeTag {};

    template <> struct enum_category<ShaderTypeFlagBits, vk::ShaderStageFlagBits> { using type = EnumShaderTypeTag; };
    template <> struct enum_category<ShaderTypeFlagBits, shaderc_shader_kind> { using type = EnumShaderTypeTag; };

    template <>
    struct enum_mapping_traits<EnumShaderTypeTag>
    {
        using mapping_type = std::tuple<ShaderTypeFlagBits, vk::ShaderStageFlagBits, shaderc_shader_kind>;
        constexpr static mapping_type mappings[] = {
            { ShaderTypeFlagBits::Vertex, vk::ShaderStageFlagBits::eVertex, shaderc_vertex_shader },
            { ShaderTypeFlagBits::TessControl, vk::ShaderStageFlagBits::eTessellationControl, shaderc_tess_control_shader },
            { ShaderTypeFlagBits::TessEvaluation, vk::ShaderStageFlagBits::eTessellationEvaluation, shaderc_tess_evaluation_shader },
            { ShaderTypeFlagBits::Geometry, vk::ShaderStageFlagBits::eGeometry, shaderc_geometry_shader },
            { ShaderTypeFlagBits::Fragment, vk::ShaderStageFlagBits::eFragment, shaderc_fragment_shader },
            { ShaderTypeFlagBits::Compute, vk::ShaderStageFlagBits::eCompute, shaderc_compute_shader },
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

    constexpr size_t size_of(ShaderDataType type)
    {
        size_t size = 0;
        switch (type) {
            case ShaderDataType::Unknown: { size = 0; } break;
            case ShaderDataType::Void: { size = 0; } break;
            case ShaderDataType::Boolean: { size = 1; } break;
            case ShaderDataType::BooleanVec2: { size = 2; } break;
            case ShaderDataType::BooleanVec3: { size = 3; } break;
            case ShaderDataType::BooleanVec4: { size = 4; } break;
            case ShaderDataType::SByte: { size = 1; } break;
            case ShaderDataType::SByteVec2: { size = 2; } break;
            case ShaderDataType::SByteVec3: { size = 3; } break;
            case ShaderDataType::SByteVec4: { size = 4; } break;
            case ShaderDataType::UByte: { size = 1; } break;
            case ShaderDataType::UByteVec2: { size = 2; } break;
            case ShaderDataType::UByteVec3: { size = 3; } break;
            case ShaderDataType::UByteVec4: { size = 4; } break;
            case ShaderDataType::Short: { size = 2; } break;
            case ShaderDataType::ShortVec2: { size = 4; } break;
            case ShaderDataType::ShortVec3: { size = 6; } break;
            case ShaderDataType::ShortVec4: { size = 8; } break;
            case ShaderDataType::UShort: { size = 2; } break;
            case ShaderDataType::UShortVec2: { size = 4; } break;
            case ShaderDataType::UShortVec3: { size = 6; } break;
            case ShaderDataType::UShortVec4: { size = 8; } break;
            case ShaderDataType::Int: { size = 4; } break;
            case ShaderDataType::IntVec2: { size = 8; } break;
            case ShaderDataType::IntVec3: { size = 12; } break;
            case ShaderDataType::IntVec4: { size = 16; } break;
            case ShaderDataType::UInt: { size = 4; } break;
            case ShaderDataType::UIntVec2: { size = 8; } break;
            case ShaderDataType::UIntVec3: { size = 12; } break;
            case ShaderDataType::UIntVec4: { size = 16; } break;
            case ShaderDataType::Int64: { size = 8; } break;
            case ShaderDataType::Int64Vec2: { size = 16; } break;
            case ShaderDataType::Int64Vec3: { size = 24; } break;
            case ShaderDataType::Int64Vec4: { size = 32; } break;
            case ShaderDataType::UInt64: { size = 8; } break;
            case ShaderDataType::UInt64Vec2: { size = 16; } break;
            case ShaderDataType::UInt64Vec3: { size = 24; } break;
            case ShaderDataType::UInt64Vec4: { size = 32; } break;
            case ShaderDataType::AtomicCounter: { size = 4; } break;
            case ShaderDataType::Half: { size = 2; } break;
            case ShaderDataType::HalfVec2: { size = 4; } break;
            case ShaderDataType::HalfVec3: { size = 6; } break;
            case ShaderDataType::HalfVec4: { size = 8; } break;
            case ShaderDataType::Float: { size = 4; } break;
            case ShaderDataType::FloatVec2: { size = 8; } break;
            case ShaderDataType::FloatVec3: { size = 12; } break;
            case ShaderDataType::FloatVec4: { size = 16; } break;
            case ShaderDataType::Double: { size = 8; } break;
            case ShaderDataType::DoubleVec2: { size = 16; } break;
            case ShaderDataType::DoubleVec3: { size = 24; } break;
            case ShaderDataType::DoubleVec4: { size = 32; } break;
            case ShaderDataType::Struct: { size = 0; } break;
            case ShaderDataType::Image: { size = 0; } break;
            case ShaderDataType::SampledImage: { size = 0; } break;
            case ShaderDataType::Sampler: { size = 0; } break;
            case ShaderDataType::AccelerationStructure: { size = 0; } break;
            case ShaderDataType::RayQuery: { size = 0; } break;
            case ShaderDataType::ControlPointArray: { size = 0; } break;
            case ShaderDataType::Interpolant: { size = 0; } break;
            case ShaderDataType::Char: { size = 1; } break;
        }
        return size;
    }

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

    struct EnumShaderResourceTypeTag {};

    template <> struct enum_category<ShaderResourceType, vk::DescriptorType> { using type = EnumShaderResourceTypeTag; };

    template <>
    struct enum_mapping_traits<EnumShaderResourceTypeTag>
    {
        using mapping_type = std::tuple<ShaderResourceType, vk::DescriptorType>;
        constexpr static mapping_type mappings[] = {
            { ShaderResourceType::Sampler, vk::DescriptorType::eSampler },
            { ShaderResourceType::CombinedImageSampler, vk::DescriptorType::eCombinedImageSampler },
            { ShaderResourceType::SampledImage, vk::DescriptorType::eSampledImage },
            { ShaderResourceType::StorageImage, vk::DescriptorType::eStorageImage },
            { ShaderResourceType::UniformTexelBuffer, vk::DescriptorType::eUniformTexelBuffer },
            { ShaderResourceType::StorageTexelBuffer, vk::DescriptorType::eStorageTexelBuffer },
            { ShaderResourceType::UniformBuffer, vk::DescriptorType::eUniformBuffer }
        };
    };
}