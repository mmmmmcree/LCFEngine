#version 460
#extension GL_EXT_shader_16bit_storage : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_debug_printf : require
#extension GL_EXT_spirv_intrinsics : require

#include "camera_uniform.glsl"

struct Vertex
{
    vec3 position;
    vec3 normal;
    vec2 uv;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer
{ 
	Vertex vertices[];
};

layout(buffer_reference, std430) readonly buffer IndexBuffer
{ 
	uint indices[];
};

layout(std430, set = 1, binding = 0) readonly buffer vertex_buffers_ssbo {
    VertexBuffer vertex_buffers[];
};

layout(std430, set = 1, binding = 1) readonly buffer index_buffer_ssbo {
    IndexBuffer index_buffers[];
};

layout(std430, set = 1, binding = 2) readonly buffer transform_ssbo {
    mat4 transform_matrices[];
};

layout(std140, set = 1, binding = 3) readonly buffer material_index_ssbo {
    uint material_indices[];
};

layout(location = 0) out VS_OUT {
    vec3 color;
    vec2 uv;
    flat uint material_id;
} vs_out;

void main()
{
    uint object_id = gl_DrawID;
    uint instance_id = gl_BaseInstance + gl_InstanceIndex;
    VertexBuffer vertex_buffer = vertex_buffers[object_id];
    IndexBuffer index_buffer = index_buffers[object_id];

    uint vertex_id = index_buffer.indices[gl_VertexIndex];
    Vertex vertex = vertex_buffer.vertices[vertex_id];

    gl_Position = projection_view * transform_matrices[instance_id] * vec4(vertex.position, 1.0);

    vs_out.color = normalize(vertex.position);
    vs_out.uv = vertex.uv;
    vs_out.material_id = material_indices[object_id];
    // if (vertex_id == 0) {
    //     // debugPrintfEXT("Position = %v4f\n", gl_Position);
    //     debugPrintfEXT("gl_DrawID = %u\n", gl_DrawID);
    //     debugPrintfEXT("texture_id = %u\n", vs_out.texture_id);
    // }
}