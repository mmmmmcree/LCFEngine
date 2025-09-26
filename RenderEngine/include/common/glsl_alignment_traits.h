#pragma once

#include "lcf_type_traits.h"

namespace lcf::render::glsl::std140 {
    class Vector2D;
    class Vector3D;
    class Vector4D;
}

namespace lcf::render::glsl::std430 {
    class Vector2D;
    class Vector3D;
    class Vector4D;
}

namespace lcf {
    //- GLSL std140 alignment traits
    template <> struct size_of<render::glsl::std140::Vector2D> : std::integral_constant<size_t, 8> {};
    template <> struct alignment_of<render::glsl::std140::Vector2D> : std::integral_constant<size_t, 8> {};

    template <> struct size_of<render::glsl::std140::Vector3D> : std::integral_constant<size_t, 12> {};
    template <> struct alignment_of<render::glsl::std140::Vector3D> : std::integral_constant<size_t, 16> {};

    template <> struct size_of<render::glsl::std140::Vector4D> : std::integral_constant<size_t, 16> {};
    template <> struct alignment_of<render::glsl::std140::Vector4D> : std::integral_constant<size_t, 16> {};
}