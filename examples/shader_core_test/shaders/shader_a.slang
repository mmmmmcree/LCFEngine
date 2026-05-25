// shader_a.slang —— import common，独立 entry point。
import common;

[shader("vertex")]
VSOutput vs_main(VSInput input)
{
    VSOutput o;
    o.sv_position = float4(input.position, 1.0);
    o.color = commonTint(float3(1.0, 0.0, 0.0));
    return o;
}

[shader("fragment")]
float4 fs_main(VSOutput input) : SV_Target
{
    return float4(input.color, 1.0);
}
