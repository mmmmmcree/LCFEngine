#pragma once

#include "common/render_enums.h"
#include "shader_core/shader_core_enums.h"
#include "image/image_enums.h"
#include <vulkan/vulkan_enums.hpp>
#include "enums/enum_cast.h"

namespace lcf::render::vkenums {
    enum class DescriptorSetStrategy : uint8_t //- only 2bits is used
    {
        eIndividual = 0,   // Long-lived, individually freeable
        eBindless = 1,   // Descriptor indexing, update-after-bind
    };

namespace internal {
    inline constexpr uint8_t encode(DescriptorSetStrategy strategy, uint8_t index) noexcept
    {
        // [strategy:2][set index:6]
        return (std::to_underlying(strategy) << 6) | (index & 0x3F);
    }
}

    enum class DescriptorSetIndex : uint8_t
    {
        ePerView = internal::encode(DescriptorSetStrategy::eIndividual, 0),
        eBindlessBuffers = internal::encode(DescriptorSetStrategy::eBindless, 1),
        eBindlessTextures = internal::encode(DescriptorSetStrategy::eBindless, 2),
    };

    enum class BindlessSetType : uint8_t //- only 1bits is used
    {
        eBuffer = 0,
        eTexture = 1
    };

    enum class BindlessBufferBinding : uint8_t //- only 3bits is used, reserve 4 bits for future use
    {
        eVertexBufferAddresses,
        eIndexBufferAddresses,
        eTransforms,
        eMaterialRecords,
        // eUniformBufferAddresses, 
        // eStorageBufferAddresses
    };

    enum class BindlessTextureBinding : uint8_t //- only 3bits is used, reserve 4 bits for future use
    {
        eSamplers,
        // eTextures3D,
        eTextureCubes,
        // eTexturesCubeArray,
        // eTextures2DArray,
        // eStorageImages2D,
        // eStorageImages3D,
        eTexture2Ds   // LAST → variable descriptor count
    };
namespace internal {
    inline constexpr uint16_t encode(DescriptorSetIndex index, uint8_t binding_point) noexcept
    {
        return (std::to_underlying(index) << 8) | binding_point;
    }

    inline constexpr uint16_t encode(DescriptorSetIndex index, BindlessBufferBinding binding_point) noexcept
    {
        return encode(index, std::to_underlying(binding_point));
    }
    inline constexpr uint16_t encode(DescriptorSetIndex index, BindlessTextureBinding binding_point) noexcept
    {
        return encode(index, std::to_underlying(binding_point));
    }
}

    enum class DescriptorBindingPoint : uint16_t
    {
        // Per-view
        eCamera = internal::encode(DescriptorSetIndex::ePerView, 0),
        // Bindless buffers
        eVertexBufferAddresses = internal::encode(DescriptorSetIndex::eBindlessBuffers, BindlessBufferBinding::eVertexBufferAddresses),
        eIndexBufferAddresses = internal::encode(DescriptorSetIndex::eBindlessBuffers, BindlessBufferBinding::eIndexBufferAddresses),
        eTransforms = internal::encode(DescriptorSetIndex::eBindlessBuffers, BindlessBufferBinding::eTransforms),
        eMaterialRecords = internal::encode(DescriptorSetIndex::eBindlessBuffers, BindlessBufferBinding::eMaterialRecords),
        // Bindless textures
        eSamplers = internal::encode(DescriptorSetIndex::eBindlessTextures, BindlessTextureBinding::eSamplers),
        eTextureCubes = internal::encode(DescriptorSetIndex::eBindlessTextures, BindlessTextureBinding::eTextureCubes),
        eTexture2Ds = internal::encode(DescriptorSetIndex::eBindlessTextures, BindlessTextureBinding::eTexture2Ds),
    };

namespace decode {
    inline constexpr DescriptorSetStrategy get_strategy(DescriptorSetIndex index) noexcept
    {
        return static_cast<DescriptorSetStrategy>(std::to_underlying(index) >> 6);
    }
    inline constexpr uint8_t get_index(DescriptorSetIndex index) noexcept
    {
        return std::to_underlying(index) & 0x3F;
    }
    inline constexpr uint8_t get_index(DescriptorBindingPoint binding_point) noexcept
    {
        return get_index(static_cast<DescriptorSetIndex>(std::to_underlying(binding_point) >> 8));
    }
    inline constexpr uint8_t get_binding_point(DescriptorBindingPoint binding_point) noexcept
    {
        return std::to_underlying(binding_point) & 0xFF;
    }
}
}

namespace lcf {
    template <>
    struct enum_mapping_traits <ShaderTypeFlagBits, vk::ShaderStageFlagBits>
    {
        static constexpr std::tuple<ShaderTypeFlagBits, vk::ShaderStageFlagBits> mappings[] = {
            { ShaderTypeFlagBits::eVertex, vk::ShaderStageFlagBits::eVertex },
            { ShaderTypeFlagBits::eTessControl, vk::ShaderStageFlagBits::eTessellationControl },
            { ShaderTypeFlagBits::eTessEvaluation, vk::ShaderStageFlagBits::eTessellationEvaluation },
            { ShaderTypeFlagBits::eGeometry, vk::ShaderStageFlagBits::eGeometry },
            { ShaderTypeFlagBits::eFragment, vk::ShaderStageFlagBits::eFragment },
            { ShaderTypeFlagBits::eCompute, vk::ShaderStageFlagBits::eCompute },
        };
    };

    template <>
    struct enum_mapping_traits<ShaderDataType, vk::Format>
    {
        static constexpr std::tuple<ShaderDataType, vk::Format> mappings[] = {
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
            { ShaderDataType::AtomicCounter, vk::Format::eUndefined },
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
        };
    };

    template <>
    struct enum_mapping_traits<ImageFormat, vk::Format>
    {
        static constexpr std::tuple<ImageFormat, vk::Format> mappings[] = {
            { ImageFormat::eInvalid, vk::Format::eUndefined },
            { ImageFormat::eGray8Uint, vk::Format::eR8Unorm },
            { ImageFormat::eGray16Uint, vk::Format::eR16Uint },
            { ImageFormat::eGray16Float, vk::Format::eR16Sfloat },
            { ImageFormat::eGray32Float, vk::Format::eR32Sfloat },
            { ImageFormat::eGrayAlpha8Uint, vk::Format::eR8G8Unorm },
            { ImageFormat::eGrayAlpha16Uint, vk::Format::eR16G16Uint },
            { ImageFormat::eGrayAlpha16Float, vk::Format::eR16G16Sfloat },
            { ImageFormat::eGrayAlpha32Float, vk::Format::eR32G32Sfloat },
            { ImageFormat::eRGB8Uint, vk::Format::eR8G8B8Unorm },
            { ImageFormat::eRGB16Uint, vk::Format::eR16G16B16Uint },
            { ImageFormat::eRGB16Float, vk::Format::eR16G16B16Sfloat },
            { ImageFormat::eRGB32Float, vk::Format::eR32G32B32Sfloat },
            { ImageFormat::eRGBA8Uint, vk::Format::eR8G8B8A8Unorm },
            { ImageFormat::eRGBA16Uint, vk::Format::eR16G16B16A16Uint },
            { ImageFormat::eRGBA16Float, vk::Format::eR16G16B16A16Sfloat },
            { ImageFormat::eRGBA32Float, vk::Format::eR32G32B32A32Sfloat },
            { ImageFormat::eBGR8Uint, vk::Format::eB8G8R8Unorm },
            { ImageFormat::eBGRA8Uint, vk::Format::eB8G8R8A8Unorm },
            { ImageFormat::eARGB8Uint, vk::Format::eUndefined },
            { ImageFormat::eCMYK8Uint, vk::Format::eUndefined },
            { ImageFormat::eYCbCr8Uint, vk::Format::eUndefined },
            { ImageFormat::eYCCK8Uint, vk::Format::eUndefined },
        };
    };
}