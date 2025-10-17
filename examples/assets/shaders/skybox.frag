#version 460
layout(location = 0) out vec4 frag_color;

layout(location = 0) in VS_OUT {
    vec3 uvw;
} fs_in;

layout(set = 2, binding = 1) uniform samplerCube cube_map;

void main()
{
    frag_color = texture(cube_map, fs_in.uvw);
}