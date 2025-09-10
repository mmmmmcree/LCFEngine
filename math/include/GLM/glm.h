#pragma once

#ifndef USE_VULKAN
    #define USE_VULKAN
#endif
//#define USE_OPENGL

#ifdef USE_VULKAN
    #define GLM_FORCE_DEPTH_ZERO_TO_ONE
#endif

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include "concepts/number_concept.h"

namespace lcf {
    template <number_c T, glm::qualifier qualifier = glm::defaultp>
    class GLMVector2D;

    template <number_c T, glm::qualifier qualifier = glm::defaultp>
    class GLMVector3D;

    template <number_c T, glm::qualifier qualifier = glm::defaultp>
    class GLMVector4D;

    template <number_c T, glm::qualifier qualifier = glm::defaultp>
    class GLMQuaternion;

    template <number_c T, glm::qualifier qualifier = glm::defaultp>
    using GLMMatrix3x3 = glm::mat<3, 3, T, qualifier>;

    template <number_c T, glm::qualifier qualifier = glm::defaultp>
    class GLMMatrix4x4;
}