#pragma once

#include "Vector.h"
#include <variant>

namespace lcf {
    using MaterialParam = std::variant<float, Vector2D<float>, Vector3D<float>, Vector4D<float>>;
}