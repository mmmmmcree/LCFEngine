#version 460
#extension GL_EXT_shader_16bit_storage : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_debug_printf : require
#extension GL_EXT_spirv_intrinsics : require

struct Vertex
{
    vec3 position;
    vec3 normal;
    vec2 uv;
};

struct ObjectData
{
    mat4 model;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer
{ 
	Vertex vertices[];
};

layout(buffer_reference, std430) readonly buffer IndexBuffer
{ 
	uint16_t indices[];
};

layout(set = 0, binding = 0) uniform camera_uniforms {
    mat4 projection;
    mat4 view;
    mat4 projection_view;
};

layout(std430, set = 1, binding = 0) readonly buffer vertex_buffers_ssbo {
    VertexBuffer vertex_buffers[];
};

layout(std430, set = 1, binding = 1) readonly buffer index_buffer_ssbo {
    IndexBuffer index_buffers[];
};

layout(std430, set = 1, binding = 2) readonly buffer object_uniforms {
    ObjectData object_data_list[];
};

layout(location = 0) out VS_OUT {
    vec3 color;
    vec2 uv;
} vs_out;

void main()
{
    uint geometry_id = gl_DrawID;
    uint base_instance_id = gl_BaseInstance;
    VertexBuffer vertex_buffer = vertex_buffers[geometry_id];
    IndexBuffer index_buffer = index_buffers[geometry_id];
    uint vertex_id = uint(index_buffer.indices[gl_VertexIndex]);
    Vertex vertex = vertex_buffer.vertices[vertex_id];
    ObjectData object_data = object_data_list[base_instance_id + gl_InstanceIndex];

    gl_Position = projection_view * object_data.model * vec4(vertex.position, 1.0);

    vs_out.color = normalize(vertex.position);
    vs_out.uv = vertex.uv;
    // if (vertex_id == 0) {
    //     debugPrintfEXT("Position = %v4f\n", gl_Position);
    // }
}