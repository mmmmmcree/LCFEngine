#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_16bit_storage : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference : require

// 间接绘制路径专用 vertex shader（CpuIndirect / GpuDriven 共用）。
// 与 examples/assets/shaders/vertex_buffer_test.vert 等价 —— fork 一份是为了让
// fragment 端可以独立扩 MaterialTextureIds 的槽位结构（5 槽 + occlusion）而不波及
// main_example 共用的 vertex_buffer_test.*。
//
// 路径关键不变量：
//   - gl_DrawID 索引 draw_meta_infos[]（cpu/gpu 路径都是 indirect draw count）
//   - gl_InstanceIndex = firstInstance + InstanceID；在 visible_instance_ids[] 内取真实 instance_id
//   - object_id 来自 draw_meta_infos[gl_DrawID].object_id，避免依赖 push constant

#include "camera_uniform.glsl"
#include "bindless_structs.glsl"

layout(std430, set = 1, binding = 0) readonly buffer DrawMetaInfoBuffer {
    uint draw_count;
    DrawMetaInfo draw_meta_infos[];
};

layout(std430, set = 1, binding = 1) readonly buffer ObjectInfos {
    uint64_t object_count;
    ObjectInfo object_infos[];
};

layout(std430, set = 1, binding = 2) readonly buffer VisibleInstanceBuffer {
    uint instance_count;
    uint visible_instance_ids[];
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

layout(buffer_reference, std430) readonly buffer VertexBufferAddress { Vertex vertices[]; };
layout(buffer_reference, std430) readonly buffer IndexBufferAddress  { uint   indices[]; };

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
    DrawMetaInfo draw_meta_info = draw_meta_infos[gl_DrawID];
    uint object_id   = draw_meta_info.object_id;
    uint instance_id = visible_instance_ids[gl_InstanceIndex];

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

    vs_out.world_normal    = N;
    vs_out.world_tangent   = T;
    vs_out.world_bitangent = B;
    vs_out.view_direction  = normalize(camera_position - world_pos.xyz);
}
