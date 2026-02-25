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
    vec4 emissive_color;
};

struct MaterialTextureIds {
    uint base_color_texture_id;
    uint roughness_texture_id;
    uint metallic_texture_id;
    uint reflectance_texture_id;
    uint ambient_occlusion_texture_id;
    uint clearcoat_texture_id;
    uint clearcoat_roughness_texture_id;
    uint anisotropy_texture_id;
};

layout(buffer_reference, std430) readonly buffer MaterialParamsRef { MaterialParams value; };

layout(buffer_reference, std430) readonly buffer MaterialTextureIdsRef { MaterialTextureIds value; };

struct MaterialRecord {
    MaterialParamsRef material_params_ref;
    MaterialTextureIdsRef material_texture_ids_ref;
};

layout(set = 2, binding = 0) readonly buffer material_records_ssbo {
    MaterialRecord material_records[];
};
//todo set 2 for material records, allocate new sets for samplers and textures

layout(set = 2, binding = 1) uniform sampler samplers[2];
layout(set = 2, binding = 2) uniform texture2D textures[65536]; // descriptor indexing, must be the last binding, here use a literal for cpu-side array size

void main()
{
    // uint material_id = fs_in.material_id;
    uint material_id = 0;
    MaterialRecord material_record = material_records[material_id];
    const MaterialParams material_params = material_record.material_params_ref.value;
    const MaterialTextureIds material_texture_ids = material_record.material_texture_ids_ref.value;

    vec4 texture0_color = texture(sampler2D(textures[nonuniformEXT(material_texture_ids.base_color_texture_id)], samplers[0]), fs_in.uv);
    vec4 texture1_color = texture(sampler2D(textures[nonuniformEXT(material_texture_ids.roughness_texture_id)], samplers[0]), fs_in.uv);

    // frag_color = material_params.base_color;
    frag_color = mix(texture0_color, texture1_color, 0.5);
}