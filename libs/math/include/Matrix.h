#pragma once

#include "GLM/GLMMatrix.h"
#include "concepts/number_concept.h"

namespace lcf {

    template<number_c T>
    using Matrix3x3 = GLMMatrix3x3<T>;

    template<number_c T>
    using Matrix4x4 = GLMMatrix4x4<T>;
}