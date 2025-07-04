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
#include <QDebug>