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
    vec3 tangent;
};

layout(buffer_reference, std430) readonly buffer VertexBufferAddress
{ 
	Vertex vertices[];
};

layout(buffer_reference, std430) readonly buffer IndexBufferAddress
{ 
	uint indices[];
};

struct VertexRecord
{
    VertexBufferAddress vertex_buffer;
    IndexBufferAddress index_buffer;
};

struct DrawMetaInfo
{
// vk::DrawIndirectCommand
    uint vertex_count;
    uint instance_count;
    uint first_vertex;
    uint first_instance;
// custom
    uint object_id;
};

layout(std430, set = 1, binding = 0) readonly buffer DrawMetaInfoBuffer {
    DrawMetaInfo draw_meta_infos[];
};

layout(std430, set = 1, binding = 1) readonly buffer VertexRecords {
    VertexRecord vertex_records[];
};

layout(std430, set = 1, binding = 2) readonly buffer VisibleInstanceBuffer {
    uint instance_count;
    uint visible_instance_ids[];
};

layout(std430, set = 1, binding = 3) readonly buffer TransformBuffer {
    mat4 transforms[];
};

layout(location = 0) out VS_OUT {
    vec2 uv;
    flat uint material_id;
    vec3 world_position;
    vec3 world_normal;
    vec3 world_tangent;
    vec3 world_bitangent;
    vec3 view_direction;
} vs_out;

void main()
{
    DrawMetaInfo draw_meta_info = draw_meta_infos[gl_DrawID];
    uint object_id = draw_meta_info.object_id;
    uint instance_id = visible_instance_ids[gl_BaseInstance + gl_InstanceIndex];

    VertexRecord vertex_record = vertex_records[object_id];
    Vertex vertex = vertex_record.vertex_buffer.vertices[vertex_record.index_buffer.indices[gl_VertexIndex]];

    mat4 model = transforms[instance_id];
    mat3 model_3x3 = mat3(model);
    mat3 normal_matrix = transpose(inverse(model_3x3));

    vec4 world_pos = model * vec4(vertex.position, 1.0);

    gl_Position = projection_view * world_pos;

    vs_out.uv = vertex.uv;
    vs_out.material_id = object_id;
    vs_out.world_position = world_pos.xyz;

    // Transform normal and tangent to world space
    vec3 N = normalize(normal_matrix * vertex.normal);
    vec3 T = normalize(model_3x3 * vertex.tangent);
    // Orthonormalize T with respect to N (Gram-Schmidt)
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);

    vs_out.world_normal = N;
    vs_out.world_tangent = T;
    vs_out.world_bitangent = B;
    vs_out.view_direction = normalize(camera_position - world_pos.xyz);
}
