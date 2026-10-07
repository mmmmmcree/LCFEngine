// Slang port of https://www.shadertoy.com/view/Ndc3zl, Image.
// iChannel0: current Buffer A output.
#include "common/fullscreen.slangh"
#include "common/ShaderToyParams.slangh"

[[vk::binding(0, 0)]] Texture2D<float4> iChannel0;
[[vk::binding(0, 1)]] SamplerState bufferSampler;

[shader("fragment")]
float4 fs_main(VSOut input) : SV_Target
{
    float2 fragCoord = float2(input.pos.x, iResolution.y - input.pos.y);
    float2 uv = fragCoord / iResolution.xy;
    float3 col = iChannel0.Sample(bufferSampler, float2(uv.x, 1.0 - uv.y)).rgb;
    col *= 0.5 + 0.5 * pow(16.0 * uv.x * uv.y * (1.0 - uv.x) * (1.0 - uv.y), 0.2);
    return float4(col, 1.0);
}
