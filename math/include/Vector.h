#pragma once
#include "GLM/GLMVector.h"
#include "concepts/number_concept.h"

namespace lcf {
    template<number_c T>
    using Vector2D = GLMVector2D<T>;
    template<number_c T>
    using Vector3D = GLMVector3D<T>;
    template<number_c T>
    using Vector4D = GLMVector4D<T>;
}