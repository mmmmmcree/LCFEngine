#pragma once
#include "GLM/GLMVector.h"
#include "concepts/number_concept.h"
#include "type_traits/lcf_type_traits.h"

namespace lcf {
    template<number_c T>
    using Vector2D = GLMVector2D<T>;

    template<number_c T>
    using Vector3D = GLMVector3D<T>;

    template<number_c T>
    using Vector4D = GLMVector4D<T>;
}

namespace lcf {
    template <number_c T>
    inline constexpr bool is_pointer_type_convertible_v<Vector2D<T>, T> = true;

    template <number_c T>
    inline constexpr bool is_pointer_type_convertible_v<Vector3D<T>, T> = true;

    template <number_c T>
    inline constexpr bool is_pointer_type_convertible_v<Vector4D<T>, T> = true;
}