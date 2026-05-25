// common.slang —— 被 shader_a/b/c 共享导入的公共依赖文件。
// 用于测试 manifest 级联失效：修改本文件应导致 a/b/c 三者全部 MISS。

module common;

public static const float k_pi = 3.14159265359;

public float3 commonTint(float3 base)
{
    return base * 0.5 + 0.5;
}

public struct VSInput
{
    public float3 position : POSITION;
};

public struct VSOutput
{
    public float3 color : VARYING0;
    public float4 sv_position : SV_Position;
};
