#pragma once

#include "common/render_enums.h"
#include "shader_core/shader_core_enums.h"
#include <vulkan/vulkan_enums.hpp>
#include "enums/enum_cast.h"

namespace lcf::render {

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
}