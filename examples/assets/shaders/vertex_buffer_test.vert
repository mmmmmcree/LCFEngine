#version 450
#extension GL_EXT_buffer_reference : require

#if defined (VULKAN_SHADER)

struct Vertex {
	vec3 position;
	vec3 normal;
	vec2 uv;
}; 

layout(buffer_reference, std430) readonly buffer VertexBuffer { 
	Vertex vertices[];
};

layout(push_constant) uniform vs_push_constant {
    mat4 model;
    VertexBuffer vertex_buffer;
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
    Vertex vertex = vertex_buffer.vertices[gl_VertexIndex];
    vs_out.color = vertex.normal;
    vs_out.uv = vertex.uv;
    gl_Position = model * vec4(vertex.position, 1.0) ;
}