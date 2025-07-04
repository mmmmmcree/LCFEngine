#version 440

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 2) in vec2 uv;

layout(location = 0) out VS_OUT {
    vec3 color;
    vec2 uv;
} vs_out;

layout(std140, binding = 0) uniform buf {
    mat4 mvp;
};

layout(push_constant) uniform Uniforms {
    mat4 model;
};

void main()
{
    vs_out.color = color;
    vs_out.uv = uv;
    gl_Position =  model * vec4(position, 1.0);
}