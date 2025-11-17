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
    template <> struct size_of<typename glsl::std140::Vector2D> : std::integral_constant<size_t, 8> {};
    template <> struct alignment_of<typename glsl::std140::Vector2D> : std::integral_constant<size_t, 8> {};

    template <> struct size_of<typename glsl::std140::Vector3D> : std::integral_constant<size_t, 12> {};
    template <> struct alignment_of<typename glsl::std140::Vector3D> : std::integral_constant<size_t, 16> {};

    template <> struct size_of<typename glsl::std140::Vector4D> : std::integral_constant<size_t, 16> {};
    template <> struct alignment_of<typename glsl::std140::Vector4D> : std::integral_constant<size_t, 16> {};
}