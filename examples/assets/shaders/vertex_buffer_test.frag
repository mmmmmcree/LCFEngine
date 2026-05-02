#version 460
layout(location = 0) out vec4 frag_color;

layout(location = 0) in VS_OUT {
    vec2 uv;
    flat uint material_id;
    vec3 world_position;
    vec3 world_normal;
    vec3 world_tangent;
    vec3 world_bitangent;
    vec3 view_direction;
} fs_in;

#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_debug_printf : require
#extension GL_EXT_buffer_reference : require

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

struct MaterialRecord {
    MaterialParamsAddress material_params;
    MaterialTextureIdsAddress material_texture_ids;
};

layout(std430, set = 1, binding = 3) readonly buffer MaterialRecords {
    MaterialRecord material_records[];
};

#pragma lcf descriptor_set_strategy(set = 2, strategy = bindless)

layout(set = 2, binding = 0) uniform sampler samplers[32];
layout(set = 2, binding = 1) uniform textureCube texture_cubes[64];
layout(set = 2, binding = 2) uniform texture2D textures[];

// Sky light direction (from surface to light, i.e. pointing at the sky)
const vec3 sky_light_dir = vec3(0.0, 1.0, 0.0);
// Sky light color (warm sunlight)
const vec3 sky_light_color = vec3(1.0, 0.98, 0.95);
// Ambient color (sky blue ambient)
const vec3 ambient_color = vec3(0.4, 0.5, 0.6);
// Ambient intensity
const float ambient_intensity = 0.35;

void main()
{
    uint material_id = fs_in.material_id;
    MaterialRecord material_record = material_records[material_id];
    const MaterialParams material_params = material_record.material_params.value;
    const MaterialTextureIds material_texture_ids = material_record.material_texture_ids.value;

    // Sample base color
    vec4 base_color = texture(sampler2D(textures[nonuniformEXT(material_texture_ids.base_color_texture_id)], samplers[0]), fs_in.uv);
    base_color *= material_params.base_color;

    // Construct TBN matrix from world-space T, B, N
    vec3 N = normalize(fs_in.world_normal);
    vec3 T = normalize(fs_in.world_tangent);
    vec3 B = normalize(fs_in.world_bitangent);
    mat3 TBN = mat3(T, B, N);  // columns: T, B, N

    // Sample normal map and transform from tangent space to world space
    vec3 tangent_normal = texture(sampler2D(textures[nonuniformEXT(material_texture_ids.normal_texture_id)], samplers[0]), fs_in.uv).xyz * 2.0 - 1.0;
    vec3 world_normal = normalize(TBN * tangent_normal);

    // Simple Lambert diffuse with sky light
    float NdotL = max(dot(world_normal, sky_light_dir), 0.0);
    vec3 diffuse = NdotL * sky_light_color * base_color.rgb;
    vec3 ambient = ambient_color * ambient_intensity * base_color.rgb;

    vec3 final_color = ambient + diffuse;

    // Add emissive
    vec4 emissive_sample = texture(sampler2D(textures[nonuniformEXT(material_texture_ids.emissive_texture_id)], samplers[0]), fs_in.uv);
    final_color += material_params.emissive_color.rgb * emissive_sample.rgb;

    frag_color = vec4(final_color, base_color.a);
}
