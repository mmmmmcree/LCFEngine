// shader_b.slang —— import common，独立 entry point（结构同 a 但内容不同，确保 SPIR-V 不重复）。
import common;

[shader("vertex")]
VSOutput vs_main(VSInput input)
{
    VSOutput o;
    o.sv_position = float4(input.position * float(2.0), 1.0);
    o.color = commonTint(float3(0.0, 1.0, 0.0));
    return o;
}

[shader("fragment")]
float4 fs_main(VSOutput input) : SV_Target
{
    return float4(input.color * float(k_pi) / float(4.0), 1.0);
}
