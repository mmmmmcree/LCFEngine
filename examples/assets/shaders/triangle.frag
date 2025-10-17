#version 440
layout(location = 0) out vec4 frag_color;

layout(location = 0) in VS_OUT {
    vec3 color;
    vec2 uv;
} fs_in;

// layout(binding = 1) uniform sampler2D tex;

void main()
{
    frag_color = texture(tex, fs_in.uv);
}