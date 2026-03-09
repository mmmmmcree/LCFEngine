#pragma once

#include "JSON.h"
#include "GLMVector.h"
#include "GLMQuaternion.h"
#include "GLMMatrix.h"
#include <format>

namespace lcf {
    template <number_c T, glm::qualifier qualifier>
    OrderedJSON serialize(const GLMVector2D<T, qualifier> & vec)
    {
        return OrderedJSON
        {
            { "x", vec.x },
            { "y", vec.y }
        };
    }

    template <number_c T, glm::qualifier qualifier>
    OrderedJSON serialize(const GLMVector3D<T, qualifier> & vec)
    {
        return OrderedJSON
        {
            { "x", vec.x },
            { "y", vec.y },
            { "z", vec.z }
        };
    }

    template <number_c T, glm::qualifier qualifier>
    OrderedJSON serialize(const GLMVector4D<T, qualifier> & vec)
    {
        return OrderedJSON
        {
            { "x", vec.x },
            { "y", vec.y },
            { "z", vec.z },
            { "w", vec.w }
        };
    }

    template <number_c T, glm::qualifier qualifier>
    OrderedJSON serialize(const GLMQuaternion<T, qualifier> & quat)
    {
        return OrderedJSON
        {
            { "x", quat.x },
            { "y", quat.y },
            { "z", quat.z },
            { "scalar", quat.w }
        };
    }

    template <number_c T, glm::qualifier qualifier>
    OrderedJSON serialize(const GLMMatrix3x3<T, qualifier> & mat)
    {
        return OrderedJSON
        {
            {mat[0][0], mat[0][1], mat[0][2]},
            {mat[1][0], mat[1][1], mat[1][2]},
            {mat[2][0], mat[2][1], mat[2][2]}
        };
    }

    template <number_c T, glm::qualifier qualifier>
    OrderedJSON serialize(const GLMMatrix4x4<T, qualifier> & mat)
    {
        return OrderedJSON
        {
            {mat[0][0], mat[0][1], mat[0][2], mat[0][3]},
            {mat[1][0], mat[1][1], mat[1][2], mat[1][3]},
            {mat[2][0], mat[2][1], mat[2][2], mat[2][3]},
            {mat[3][0], mat[3][1], mat[3][2], mat[3][3]}
        };
    }

    template <number_c T, glm::qualifier qualifier>
    std::string to_string(const GLMMatrix4x4<T, qualifier> &mat)
    {
        return std::format("GLMTransformMatrix(\n"
            "  {:7.4f}, {:7.4f}, {:7.4f}, {:7.4f},\n"
            "  {:7.4f}, {:7.4f}, {:7.4f}, {:7.4f},\n"
            "  {:7.4f}, {:7.4f}, {:7.4f}, {:7.4f},\n"
            "  {:7.4f}, {:7.4f}, {:7.4f}, {:7.4f}\n)",
            mat[0][0], mat[1][0], mat[2][0], mat[3][0],
            mat[0][1], mat[1][1], mat[2][1], mat[3][1],
            mat[0][2], mat[1][2], mat[2][2], mat[3][2],
            mat[0][3], mat[1][3], mat[2][3], mat[3][3]
        );       
    }
}