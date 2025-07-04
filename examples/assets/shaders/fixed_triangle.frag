#version 440
layout(location = 0) out vec4 frag_color;

layout(location = 0) in VS_OUT {
    vec3 color;
} fs_in;

layout(binding = 1) uniform sampler2D tex;

void main()
{
    frag_color = vec4(fs_in.color,1.0);
}