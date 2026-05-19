#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

layout(location = 0) out vec4 frag_color;

layout(location = 0) in VS_OUT {
    vec2 uv;
    flat uint object_id;
    vec3 world_position;
    vec3 world_normal;
    vec3 world_tangent;
    vec3 world_bitangent;
    vec3 view_direction;
} fs_in;

#include "bindless_structs.glsl"

struct MaterialParams {
    vec4 base_color;
    float roughness;
    float metallic;
    float reflectance;
    float ambient_occlusion;
    vec4 emissive_color;
    vec3 normal;
};

struct MaterialTextureIds {
    uint base_color_texture_id;
    uint metallic_roughness_texture_id;
    uint normal_texture_id;
    uint emissive_texture_id;
};

layout(buffer_reference, std430) readonly buffer MaterialParamsAddress { MaterialParams value; };
layout(buffer_reference, std430) readonly buffer MaterialTextureIdsAddress { MaterialTextureIds value; };

layout(std430, set = 1, binding = 1) readonly buffer ObjectInfos {
    uint64_t object_count;
    ObjectInfo object_infos[];
};

#pragma lcf descriptor_set_strategy(set = 2, strategy = bindless)
layout(set = 2, binding = 0) uniform sampler samplers[32];
layout(set = 2, binding = 1) uniform textureCube texture_cubes[64];
layout(set = 2, binding = 2) uniform texture2D textures[];

const vec3 sky_light_dir = vec3(0.0, 1.0, 0.0);
const vec3 sky_light_color = vec3(1.0, 0.98, 0.95);
const vec3 ambient_color = vec3(0.4, 0.5, 0.6);
const float ambient_intensity = 0.35;

void main()
{
    ObjectInfo object_info = object_infos[fs_in.object_id];
    const MaterialParams material_params = MaterialParamsAddress(object_info.material_params).value;
    const MaterialTextureIds material_texture_ids = MaterialTextureIdsAddress(object_info.material_texture_ids).value;

    vec4 base_color = texture(sampler2D(textures[nonuniformEXT(material_texture_ids.base_color_texture_id)], samplers[0]), fs_in.uv);
    base_color *= material_params.base_color;

    vec3 N = normalize(fs_in.world_normal);
    vec3 T = normalize(fs_in.world_tangent);
    vec3 B = normalize(fs_in.world_bitangent);
    mat3 TBN = mat3(T, B, N);

    vec3 tangent_normal = texture(sampler2D(textures[nonuniformEXT(material_texture_ids.normal_texture_id)], samplers[0]), fs_in.uv).xyz * 2.0 - 1.0;
    vec3 world_normal = normalize(TBN * tangent_normal);

    float NdotL = max(dot(world_normal, sky_light_dir), 0.0);
    vec3 diffuse = NdotL * sky_light_color * base_color.rgb;
    vec3 ambient = ambient_color * ambient_intensity * base_color.rgb;

    vec3 final_color = ambient + diffuse;

    vec4 emissive_sample = texture(sampler2D(textures[nonuniformEXT(material_texture_ids.emissive_texture_id)], samplers[0]), fs_in.uv);
    final_color += material_params.emissive_color.rgb * emissive_sample.rgb;

    frag_color = vec4(final_color, base_color.a);
}
