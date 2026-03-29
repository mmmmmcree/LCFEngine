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

layout(buffer_reference, std430) readonly buffer VertexBufferAddress
{ 
	Vertex vertices[];
};

layout(buffer_reference, std430) readonly buffer IndexBufferAddress
{ 
	uint indices[];
};

#pragma lcf descriptor_set_strategy(set = 1, strategy = bindless);

layout(std430, set = 1, binding = 0) readonly buffer VertexBufferAddresses {
    VertexBufferAddress vertex_buffer[];
};

layout(std430, set = 1, binding = 1) readonly buffer IndexBufferAddresses {
    IndexBufferAddress index_buffer[];
};

layout(std430, set = 1, binding = 2) readonly buffer TransformBuffer {
    mat4 transforms[];
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

    uint vertex_id = index_buffer[object_id].indices[gl_VertexIndex];
    Vertex vertex = vertex_buffer[object_id].vertices[vertex_id];

    gl_Position = projection_view * transforms[instance_id] * vec4(vertex.position, 1.0);

    vs_out.color = normalize(vertex.position);
    vs_out.uv = vertex.uv;
    vs_out.material_id = object_id;
    // if (vertex_id == 0) {
        // debugPrintfEXT("Position = %v4f\n", gl_Position);
    //     debugPrintfEXT("gl_DrawID = %u\n", gl_DrawID);
    //     debugPrintfEXT("texture_id = %u\n", vs_out.texture_id);
    // }
}