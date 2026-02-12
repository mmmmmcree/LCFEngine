#pragma once

#include "Vector.h"

namespace lcf::constants {
    constexpr Vector3D<float> quad_positions[] = {
        {-1.0f, -1.0f, 0.0f},
        {+1.0f, -1.0f, 0.0f},
        {+1.0f, +1.0f, 0.0f},
        {-1.0f, +1.0f, 0.0f}
    };
    constexpr Vector3D<float> quad_colors[] = {
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},
        {1.0f, 1.0f, 1.0f}
    };
    constexpr Vector2D<float> quad_uvs[] = {
        {0.0f, 1.0f},
        {1.0f, 1.0f},
        {1.0f, 0.0f},
        {0.0f, 0.0f}
    };
    constexpr uint16_t quad_indices[] = { 0, 1, 2, 2, 3, 0 };

    constexpr Vector3D<float> cube_positions[] = {
        {-1.0f, -1.0f, -1.0f}, {-1.0f, +1.0f, -1.0f}, {+1.0f, +1.0f, -1.0f}, {+1.0f, -1.0f, -1.0f}, //- back face
        {-1.0f, -1.0f, -1.0f}, {-1.0f, -1.0f, +1.0f}, {+1.0f, -1.0f, +1.0f}, {+1.0f, -1.0f, -1.0f}, //- left face
        {+1.0f, -1.0f, -1.0f}, {+1.0f, -1.0f, +1.0f}, {+1.0f, +1.0f, +1.0f}, {+1.0f, +1.0f, -1.0f}, //- right face
        {+1.0f, +1.0f, -1.0f}, {+1.0f, +1.0f, +1.0f}, {-1.0f, +1.0f, +1.0f}, {-1.0f, +1.0f, -1.0f}, //- top face
        {-1.0f, +1.0f, -1.0f}, {-1.0f, +1.0f, +1.0f}, {-1.0f, -1.0f, +1.0f}, {-1.0f, -1.0f, -1.0f}, //- bottom face
        {-1.0f, -1.0f, +1.0f}, {+1.0f, -1.0f, +1.0f}, {+1.0f, +1.0f, +1.0f}, {-1.0f, +1.0f, +1.0f}, //- front face
    };
    constexpr Vector3D<float> cube_normals[] = {
        {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, //- back face
        {-1.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, //- left face
        {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, //- right face
        {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, //- top face
        {0.0f, -1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, //- bottom face
        {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, //- front face
    };
    constexpr Vector2D<float> cube_uvs[] = {
        {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f}, //- back face
        {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f}, //- left face
        {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f}, //- right face
        {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f}, //- top face
        {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f}, //- bottom face
        {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f}, //- front face
    };
    constexpr uint16_t cube_indices[] = {
        2, 0, 1, 2, 3, 0, //- back face
        4, 6, 5, 4, 7, 6, //- left face
        8, 10 ,9, 8, 11, 10, //- right face
        12, 14, 13, 12, 15, 14, //- top face
        16, 18,17, 16, 19, 18, //- bottom face
        20, 21, 22, 20, 22, 23 //- front face
    };
}