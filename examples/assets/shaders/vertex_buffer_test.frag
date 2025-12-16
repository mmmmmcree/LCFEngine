#version 460
layout(location = 0) out vec4 frag_color;

layout(location = 0) in VS_OUT {
    vec3 color;
    vec2 uv;
    flat uint material_id;
} fs_in;

#extension GL_EXT_nonuniform_qualifier : require
struct MaterialParams {
    vec4 base_color;
    vec4 emissive_color;
};

layout(set = 2, binding = 0) readonly buffer material_params_ssbo {
    MaterialParams material_params[];
};
layout(set = 2, binding = 1) uniform sampler samplers[2];
layout(set = 2, binding = 2) uniform texture2D textures[65536]; // descriptor indexing, must be the last binding, here use a literal for cpu-side array size

void main()
{
    uint material_id = fs_in.material_id;
    vec4 texture0_color = texture(sampler2D(textures[nonuniformEXT(material_id)], samplers[0]), fs_in.uv);
    vec4 texture1_color = texture(sampler2D(textures[nonuniformEXT(material_id + 1)], samplers[0]), fs_in.uv);

    frag_color = mix(texture0_color, texture1_color, 0.5);
}