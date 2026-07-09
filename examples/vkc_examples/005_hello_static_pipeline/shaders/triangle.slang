static const float2 k_positions[3] = { float2(0, -0.5), float2(0.5, 0.5), float2(-0.5, 0.5) };
static const float3 k_colors[3]    = { float3(1,0,0), float3(0,1,0), float3(0,0,1) };

struct VSOut { float4 pos : SV_Position; float3 color : COLOR0; };

[shader("vertex")]
VSOut vs_main(uint vid : SV_VertexID) {
    VSOut o; o.pos = float4(k_positions[vid], 0, 1); o.color = k_colors[vid]; return o;
}

[shader("fragment")]
float4 fs_main(VSOut i) : SV_Target { return float4(i.color, 1); }