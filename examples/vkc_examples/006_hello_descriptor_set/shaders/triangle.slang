static const float2 k_positions[6] = {
    float2(-0.7, -0.7), float2(0.7, -0.7), float2(0.7, 0.7),
    float2(-0.7, -0.7), float2(0.7, 0.7), float2(-0.7, 0.7)
};
static const float2 k_uvs[6] = {
    float2(0, 0), float2(1, 0), float2(1, 1),
    float2(0, 0), float2(1, 1), float2(0, 1)
};

[[vk::binding(0, 0)]] Texture2D<float4> texture_image;
[[vk::binding(1, 0)]] SamplerState texture_sampler;

struct VSOut
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};

[shader("vertex")]
VSOut vs_main(uint vertex_id: SV_VertexID)
{
    VSOut vs_out;
    vs_out.pos = float4(k_positions[vertex_id], 0, 1);
    vs_out.uv = k_uvs[vertex_id];
    return vs_out;
}

[shader("fragment")]
float4 fs_main(VSOut fs_in) : SV_Target
{
    return texture_image.Sample(texture_sampler, fs_in.uv);
}
