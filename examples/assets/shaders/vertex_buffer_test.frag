#version 460
layout(location = 0) out vec4 frag_color;

layout(location = 0) in VS_OUT {
    vec3 color;
    vec2 uv;
    flat uint material_id;
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
//todo set 2 for material records, allocate new sets for samplers and textures

#pragma lcf descriptor_set_strategy(set = 2, strategy = bindless)

layout(set = 2, binding = 0) uniform sampler samplers[2];
layout(set = 2, binding = 1) uniform texture2D textures[65536]; // descriptor indexing, must be the last binding, here use a literal for cpu-side array size

void main()
{
    uint material_id = fs_in.material_id;
    MaterialRecord material_record = material_records[material_id];
    const MaterialParams material_params = material_record.material_params.value;
    const MaterialTextureIds material_texture_ids = material_record.material_texture_ids.value;

    // vec4 texture0_color = texture(sampler2D(textures[nonuniformEXT(material_texture_ids.base_color_texture_id)], samplers[0]), fs_in.uv);
    // vec4 texture1_color = texture(sampler2D(textures[nonuniformEXT(material_texture_ids.metallic_roughness_texture_id)], samplers[0]), fs_in.uv);
    vec4 texture_base_color = texture(sampler2D(textures[nonuniformEXT(material_texture_ids.base_color_texture_id)], samplers[0]), fs_in.uv);
    // vec4 base_color = material_params.base_color * texture_base_color;
    vec4 base_color = texture_base_color * material_params.base_color;
    vec4 texture_emmisive_color = texture(sampler2D(textures[nonuniformEXT(material_texture_ids.emissive_texture_id)], samplers[0]), fs_in.uv);
    vec4 emissive_color = material_params.emissive_color * texture_emmisive_color;

    frag_color = texture_base_color + emissive_color;
}