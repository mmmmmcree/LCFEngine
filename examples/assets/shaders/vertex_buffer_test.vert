#version 450
#extension GL_EXT_buffer_reference : require

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;

layout(set = 0, binding = 0) uniform global_uniform_buffer {
    // mat4 view;
    // mat4 projection;
    mat4 projection_view;
};

#if defined (VULKAN_SHADER)

layout(push_constant) uniform vs_push_constant {
    mat4 model;
};

#elif defined (OPENGL_SHADER)
uniform mat4 model;
#endif

layout(location = 0) out VS_OUT {
    vec3 color;
    vec2 uv;
} vs_out;


void main()
{
    vs_out.color = normal;
    vs_out.uv = uv;
    gl_Position = projection_view * model * vec4(position, 1.0) ;
}