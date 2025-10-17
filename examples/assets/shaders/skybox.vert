#version 460 core

#include "camera_uniform.glsl"
#include "cube_data.glsl"

layout(location = 0) out VS_OUT {
    vec3 uvw;
} vs_out;

void main()
{
    vec3 position = cube_positions[cube_indices[gl_VertexIndex]];
    mat4 skybox_view = mat4(mat3(view));
    gl_Position = projection * skybox_view * vec4(position, 1.0);
    gl_Position = gl_Position.xyww;
    vs_out.uvw = position;
}