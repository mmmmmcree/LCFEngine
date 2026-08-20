static const float2 k_positions[3] = { float2(0, -0.5), float2(0.5, 0.5), float2(-0.5, 0.5) };
static const float3 k_colors[3] = { float3(1,0,0), float3(0,1,0), float3(0,0,1) };

struct VSOut
{
    float4 pos : SV_Position;
    float3 color : COLOR0;
};

[shader("vertex")]
VSOut vs_main(uint vertex_id: SV_VertexID)
{
    VSOut vs_out;
    vs_out.pos = float4(k_positions[vertex_id], 0, 1);
    vs_out.color = k_colors[vertex_id];
    return vs_out;
}

[shader("fragment")]
float4 fs_main(VSOut fs_in) : SV_Target
{
    return float4(fs_in.color, 1.0);
}