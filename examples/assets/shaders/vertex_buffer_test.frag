#version 450
layout(location = 0) out vec4 frag_color;

layout(location = 0) in VS_OUT {
    vec3 color;
    vec2 uv;
} fs_in;

layout(set = 1, binding = 0) uniform sampler2D tex;

void main()
{
    // frag_color = vec4(fs_in.color, 1.0);
    frag_color = texture(tex, fs_in.uv);
}