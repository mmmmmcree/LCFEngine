#version 460
layout(location = 0) out vec4 frag_color;

layout(location = 0) in VS_OUT {
    vec3 uvw;
} fs_in;
#extension GL_EXT_nonuniform_qualifier : require

layout(set = 2, binding = 0) uniform sampler samplers[32];
layout(set = 2, binding = 1) uniform textureCube texture_cubes[64];
layout(set = 2, binding = 2) uniform texture2D textures[];

void main()
{
    vec4 color = texture(sampler2D(textures[nonuniformEXT(0)], samplers[0]), vec2(0, 0)); //dead code
    frag_color = texture(samplerCube(texture_cubes[0], samplers[1]), fs_in.uvw);
}