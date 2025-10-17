#version 460
layout(location = 0) out vec4 frag_color;

layout(location = 0) in VS_OUT {
    vec3 color;
    vec2 uv;
} fs_in;

// layout(set = 2, binding = 1) uniform sampler2D tex;
layout(set = 2, binding = 1) uniform texture2D textures[2];
layout(set = 2, binding = 2) uniform sampler _sampler;


void main()
{
    vec4 texture1 = texture(sampler2D(textures[0], _sampler), fs_in.uv);
    vec4 texture2 = texture(sampler2D(textures[1], _sampler), fs_in.uv);
    frag_color = mix(texture1, texture2, 0.5);
}