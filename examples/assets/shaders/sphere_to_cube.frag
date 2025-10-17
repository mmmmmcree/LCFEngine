#version 460 core

layout(location = 0) out vec4 frag_color;

layout(location = 0) in GS_OUT {
    vec3 uvw;
} fs_in;

layout(set = 0, binding = 0) uniform sampler2D sphere_map;

const float PI = 3.14159265359;

vec2 spherical_uv(vec3 n_uvw) {
    float phi = asin(n_uvw.y);
    float theta = atan(n_uvw.z, n_uvw.x);
    return vec2(0.5 + 0.5 * theta / PI, phi / PI + 0.5);
}

void main()
{
    vec3 n_uvw = normalize(fs_in.uvw);
    vec2 uv = spherical_uv(n_uvw);
    frag_color = texture(sphere_map, uv);
}