// shader_c.slang —— import common，独立 entry point。
// 测试矩阵中阶段 4-5 会向本文件 append 空行验证：仅 c MISS、a/b 仍 HIT。
import common;

[shader("vertex")]
VSOutput vs_main(VSInput input)
{
    VSOutput o;
    o.sv_position = float4(-input.position, 1.0);
    o.color = commonTint(float3(0.0, 0.0, 1.0));
    return o;
}

[shader("fragment")]
float4 fs_main(VSOutput input) : SV_Target
{
    return float4(float3(1.0, 1.0, 1.0) - input.color, 1.0);
}
