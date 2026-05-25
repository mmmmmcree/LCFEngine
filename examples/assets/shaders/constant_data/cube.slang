static const float3 k_cube_positions[8] = {
    float3(-1.0, -1.0, -1.0), float3(-1.0, +1.0, -1.0), float3(+1.0, +1.0, -1.0), float3(+1.0, -1.0, -1.0),
    float3(-1.0, -1.0, +1.0), float3(-1.0, +1.0, +1.0), float3(+1.0, +1.0, +1.0), float3(+1.0, -1.0, +1.0)
};

static const uint k_cube_indices[36] = {
    2, 0, 1, 2, 3, 0, //- back face
    0, 4, 5, 0, 5, 1, //- left face
    1, 5, 6, 1, 6, 2, //- right face
    2, 6, 7, 2, 7, 3, //- top face
    3, 7, 4, 3, 4, 0, //- bottom face
    4, 7, 6, 4, 6, 5 //- front face
};