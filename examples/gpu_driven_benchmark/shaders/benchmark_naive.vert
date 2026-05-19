#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_16bit_storage : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference : require

#include "camera_uniform.glsl"
#include "bindless_structs.glsl"

// Naive CPU path：没有 cull.comp / 没有 indirect draw / 没有 gl_DrawID 间接索引。
// host 端对每个 (mesh, visible instance) 单独提交 vkCmdDraw，并在 push constant
// 里告诉 shader 当前 mesh 的 object_id。gl_InstanceIndex 直接 = host 端
// vkCmdDraw 的 firstInstance 参数（即 instance pool 里的绝对 instance_id）。

layout(push_constant) uniform NaivePushConstant {
    uint object_id;
} pc;

// 与 vertex_buffer_test.vert 共用的 set 1 SSBO 布局（只用 ObjectInfos / InstanceInfos 两路）。
layout(std430, set = 1, binding = 1) readonly buffer ObjectInfos {
    uint64_t object_count;
    ObjectInfo object_infos[];
};

layout(std430, set = 1, binding = 3) readonly buffer InstanceInfos {
    InstanceInfo instance_infos[];
};

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

layout(location = 0) out VS_OUT {
    vec2 uv;
    flat uint object_id;
    vec3 world_position;
    vec3 world_normal;
    vec3 world_tangent;
    vec3 world_bitangent;
    vec3 view_direction;
} vs_out;

void main()
{
    uint object_id = pc.object_id;
    uint instance_id = uint(gl_InstanceIndex);

    ObjectInfo object_info = object_infos[object_id];
    Vertex vertex = VertexBufferAddress(object_info.vertex_buffer)
                        .vertices[IndexBufferAddress(object_info.index_buffer).indices[gl_VertexIndex]];

    InstanceInfo instance_info = instance_infos[instance_id];
    mat4 model = instance_info.transform;
    mat3 model_3x3 = mat3(model);
    mat3 normal_matrix = transpose(inverse(model_3x3));

    vec4 world_pos = model * vec4(vertex.position, 1.0);
    gl_Position = projection_view * world_pos;

    vs_out.uv = vertex.uv;
    vs_out.object_id = object_id;
    vs_out.world_position = world_pos.xyz;

    vec3 N = normalize(normal_matrix * vertex.normal);
    vec3 T = normalize(model_3x3 * vertex.tangent);
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);

    vs_out.world_normal = N;
    vs_out.world_tangent = T;
    vs_out.world_bitangent = B;
    vs_out.view_direction = normalize(camera_position - world_pos.xyz);
}
