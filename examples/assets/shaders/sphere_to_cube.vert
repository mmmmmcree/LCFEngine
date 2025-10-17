#version 460 core
#include "cube_data.glsl"

layout (location = 0) out VS_OUT {
    vec3 uvw;
} vs_out;

void main()
{
    int index = cube_indices[gl_VertexIndex];
    vec3 position = cube_positions[index];
    vs_out.uvw = position;
    gl_Position = vec4(position, 1.0);
}