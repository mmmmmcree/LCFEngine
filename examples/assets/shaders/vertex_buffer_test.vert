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
    uint object_id = gl_DrawID;
    uint instance_id = gl_BaseInstance + gl_InstanceIndex;

    uint vertex_id = index_buffer[object_id].indices[gl_VertexIndex];
    Vertex vertex = vertex_buffer[object_id].vertices[vertex_id];

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
