static const float2 k_positions[6] = {
    float2(-1.0, -1.0), float2(1.0, -1.0), float2(1.0, 1.0),
    float2(-1.0, -1.0), float2(1.0, 1.0), float2(-1.0, 1.0)
};
static const float3 k_colors[6] = {
    float3(1,0,0), float3(0,1,0), float3(0,0,1),
    float3(1,0,0), float3(0,0,1), float3(0,1,0)
};
static const float2 k_uvs[6] = {
    float2(0, 0), float2(1, 0), float2(1, 1),
    float2(0, 0), float2(1, 1), float2(0, 1)
};

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

[[vk::push_constant]]
cbuffer ShaderToyParams
{
    float3 iResolution;
    float iTime;
};


[shader("fragment")]
float4 fs_main(VSOut fs_in) : SV_Target
{
    float4 O = float4(0.0);
    float2 I = fs_in.pos.xy;
    I.y = iResolution.y - I.y;

    float i = 0.0, z = 0.0, d = 0.0;
    for (O *= i; i++ < 80.;) {
        float3 p = z * normalize(float3(I+I, 0) - iResolution.xyy);
        p.z = p.z * .5 - iTime * 2.;
        for (d = 1.; d++ < 11.;)
            p += cos(p.yzx * d + iTime) / d;
        z += d = .01 + abs(3. - length(p.xy)) / 6.;
        O += (cos(p.z + float4(6,1,2,0)) + 1.) / d / z / 11e2;
    }
    return O;
}   