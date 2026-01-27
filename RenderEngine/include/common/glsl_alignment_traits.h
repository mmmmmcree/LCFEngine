#pragma once

#include "lcf_type_traits.h"

namespace glsl::std140 {
    class Vector2D {};
    class Vector3D {};
    class Vector4D {};
    
    struct ShaderTypeMapping
    {
        using float_t = float;
        using vec2_t = Vector2D;
        using vec3_t = Vector3D;
        using vec4_t = Vector4D;
    };
}

namespace glsl::std430 {
    class Vector2D {};
    class Vector3D {};
    class Vector4D {};
}

namespace lcf {
    //- GLSL std140 alignment traits
    template <> inline constexpr size_t size_of_v<typename glsl::std140::Vector2D> = 8;
    template <> inline constexpr size_t alignment_of_v<typename glsl::std140::Vector2D> = 8;
    template <> inline constexpr size_t size_of_v<typename glsl::std140::Vector3D> = 12;
    template <> inline constexpr size_t alignment_of_v<typename glsl::std140::Vector3D> = 16;
    template <> inline constexpr size_t size_of_v<typename glsl::std140::Vector4D> = 16;
    template <> inline constexpr size_t alignment_of_v<typename glsl::std140::Vector4D> = 16;


}