// Slang port of https://www.shadertoy.com/view/Ndc3zl, Buffer A.
// iChannel0: 1024x1024 noise texture; iChannel1: previous Buffer A frame.
#include "common/fullscreen.slangh"
#include "common/ShaderToyParams.slangh"

[[vk::binding(0, 0)]] Texture2D<float4> iChannel0;
[[vk::binding(1, 0)]] Texture2D<float4> iChannel1;
[[vk::binding(0, 1)]] SamplerState noiseSampler;
[[vk::binding(1, 1)]] SamplerState feedbackSampler;

float noise(float2 x)
{
    float2 f = frac(x);
    float2 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
    float2 p = floor(x);
    float a = iChannel0.Sample(noiseSampler, (p + float2(0.0, 0.0)) / 1024.0).x;
    float b = iChannel0.Sample(noiseSampler, (p + float2(1.0, 0.0)) / 1024.0).x;
    float c = iChannel0.Sample(noiseSampler, (p + float2(0.0, 1.0)) / 1024.0).x;
    float d = iChannel0.Sample(noiseSampler, (p + float2(1.0, 1.0)) / 1024.0).x;
    return a + (b - a) * u.x + (c - a) * u.y + (a - b - c + d) * u.x * u.y;
}

float fbm(float2 x, int detail)
{
    float a = 0.0;
    float b = 1.0;
    float t = 0.0;
    for (int i = 0; i < detail; ++i) {
        a += b * noise(x);
        t += b;
        b *= 0.7;
        x *= 2.0;
    }
    return a / t;
}

float fbm2(float2 x, int detail)
{
    float a = 0.0;
    float b = 1.0;
    float t = 0.0;
    for (int i = 0; i < detail; ++i) {
        a += b * noise(x);
        t += b;
        b *= 0.9;
        x *= 2.0;
    }
    return a / t;
}

float box(float2 uv, float x1, float x2, float y1, float y2)
{
    return (uv.x > x1 && uv.x < x2 && uv.y > y1 && uv.y < y2) ? 1.0 : 0.0;
}

float dot2(float2 v)
{
    return dot(v, v);
}

#define CLOUD_LAYER(offset, color) if (uv.y < h + midlevel - (offset)) return float4(color, 1.0)

float4 foreground(float2 uv, float t)
{
    float midlevel;
    float h;
    float disp;
    float dist;
    float2 uv2;

    uv.y -= 0.2;

    // c14
    midlevel = -0.1;
    disp = 1.7;
    dist = 1.0;
    uv2 = uv + float2(t / dist + 40.0, 0.0);
    h = (fbm(uv2, 8) - 0.5) * disp;
    CLOUD_LAYER(0.12, float3(0.43, 0.32, 0.31));
    CLOUD_LAYER(0.08, float3(0.55, 0.42, 0.41));
    CLOUD_LAYER(0.04, float3(0.66, 0.42, 0.40));
    CLOUD_LAYER(0.0, float3(0.77, 0.48, 0.46));

    // c13
    midlevel = 0.05;
    disp = 1.7;
    dist = 2.0;
    uv2 = uv + float2(t / dist + 38.0, 0.0);
    h = (fbm(uv2, 8) - 0.5) * disp;
    CLOUD_LAYER(0.1, float3(0.95, 0.66, 0.48));
    CLOUD_LAYER(0.04, float3(0.98, 0.76, 0.64));
    CLOUD_LAYER(0.0, float3(0.95, 0.80, 0.77));

    return float4(0.95, 0.80, 0.77, 0.0);
}

float4 background(float2 uv, float t)
{
    float midlevel;
    float h;
    float disp;
    float dist;
    float2 uv2;

    // c12
    midlevel = 0.3;
    disp = 0.9;
    dist = 10.0;
    uv2 = uv + float2(t / dist + 32.5, 0.0);
    h = (fbm(uv2, 8) - 0.5) * disp;
    CLOUD_LAYER(0.14, float3(0.48, 0.19, 0.20));
    CLOUD_LAYER(0.1, float3(0.68, 0.28, 0.19));
    CLOUD_LAYER(0.07, float3(0.88, 0.38, 0.24));
    CLOUD_LAYER(0.0, float3(0.95, 0.45, 0.30));

    // c11
    midlevel = 0.35;
    disp = 1.0;
    dist = 15.0;
    uv2 = uv + float2(t / dist + 30.0, 0.0);
    h = (fbm(uv2, 8) - 0.5) * disp;
    CLOUD_LAYER(0.04, float3(0.98, 0.76, 0.64));
    CLOUD_LAYER(0.0, float3(0.95, 0.80, 0.77));

    // c10
    midlevel = 0.35;
    disp = 3.5;
    dist = 20.0;
    uv2 = uv + float2(t / dist + 27.5, 0.0);
    h = (fbm(uv2, 8) - 0.5) * disp;
    CLOUD_LAYER(0.12, float3(0.43, 0.32, 0.31));
    CLOUD_LAYER(0.08, float3(0.55, 0.42, 0.41));
    CLOUD_LAYER(0.04, float3(0.66, 0.42, 0.40));
    CLOUD_LAYER(0.0, float3(0.77, 0.48, 0.46));

    // c9
    midlevel = 0.45;
    disp = 2.0;
    dist = 25.0;
    uv2 = uv + float2(t / dist + 23.0, 0.0);
    h = (fbm(uv2, 8) - 0.5) * disp;
    CLOUD_LAYER(0.04, float3(0.98, 0.57, 0.36));
    CLOUD_LAYER(0.0, float3(1.0, 0.62, 0.44));

    // c8
    midlevel = 0.5;
    disp = 2.3;
    dist = 30.0;
    uv2 = uv + float2(t / dist + 20.5, 0.0);
    h = (fbm(uv2, 8) - 0.5) * disp;
    CLOUD_LAYER(0.12, float3(0.41, 0.27, 0.27));
    CLOUD_LAYER(0.08, float3(0.53, 0.35, 0.32));
    CLOUD_LAYER(0.04, float3(0.80, 0.24, 0.17));
    CLOUD_LAYER(0.0, float3(0.99, 0.29, 0.20));

    // c7
    midlevel = 0.5;
    disp = 2.5;
    dist = 35.0;
    uv2 = uv + float2(t / dist + 18.0, 0.0);
    h = (fbm(uv2, 8) - 0.5) * disp;
    CLOUD_LAYER(0.1, float3(0.88, 0.38, 0.24));
    CLOUD_LAYER(0.05, float3(0.98, 0.42, 0.28));
    CLOUD_LAYER(0.0, float3(1.0, 0.48, 0.35));

    // c6
    midlevel = 0.6;
    disp = 2.0;
    dist = 40.0;
    uv2 = uv + float2(t / dist + 18.0, 0.0);
    h = (fbm(uv2, 8) - 0.5) * disp;
    CLOUD_LAYER(0.1, float3(0.95, 0.66, 0.48));
    CLOUD_LAYER(0.0, float3(1.0, 0.76, 0.60));

    // c5
    midlevel = 0.75;
    disp = 3.5;
    dist = 45.0;
    uv2 = uv + float2(t / dist + 15.5, 0.0);
    h = (fbm(uv2, 8) - 0.5) * disp;
    CLOUD_LAYER(0.2, float3(1.0, 0.55, 0.33));
    CLOUD_LAYER(0.15, float3(0.98, 0.50, 0.24));
    CLOUD_LAYER(0.1, float3(0.90, 0.55, 0.40));
    CLOUD_LAYER(0.0, float3(1.0, 0.62, 0.44));

    // c4
    midlevel = 0.7;
    disp = 2.7;
    dist = 50.0;
    uv2 = uv + float2(t / dist + 12.0, 0.0);
    h = (fbm(uv2, 8) - 0.5) * disp;
    CLOUD_LAYER(0.04, float3(0.73, 0.36, 0.30));
    CLOUD_LAYER(0.0, float3(0.80, 0.40, 0.34));

    // c3
    midlevel = 0.8;
    disp = 2.7;
    dist = 60.0;
    uv2 = uv + float2(t / dist + 9.5, 0.0);
    h = (fbm(uv2, 8) - 0.5) * disp;
    CLOUD_LAYER(0.1, float3(0.93, 0.58, 0.35));
    CLOUD_LAYER(0.0, float3(1.0, 0.76, 0.60));

    // c2
    midlevel = 0.9;
    disp = 3.0;
    dist = 70.0;
    uv2 = uv + float2(t / dist + 7.0, 0.0);
    h = (fbm(uv2, 8) - 0.5) * disp;
    CLOUD_LAYER(0.1, float3(0.56, 0.25, 0.22));
    CLOUD_LAYER(0.05, float3(0.60, 0.30, 0.27));
    CLOUD_LAYER(0.0, float3(0.74, 0.35, 0.30));

    // c1
    midlevel = 1.0;
    disp = 5.0;
    dist = 100.0;
    uv2 = uv + float2(t / dist + 3.5, 0.0);
    h = (fbm(uv2, 8) - 0.5) * disp;
    CLOUD_LAYER(0.1, float3(0.92, 0.85, 0.82));
    CLOUD_LAYER(0.0, float3(1.0, 0.94, 0.91));

    return float4(0.58, 0.7, 1.0, 1.0);
}

#undef CLOUD_LAYER

[shader("fragment")]
float4 fs_main(VSOut input) : SV_Target
{
    float2 fragCoord = float2(input.pos.x, iResolution.y - input.pos.y);
    float2 uv = fragCoord / iResolution.y;
    float t = iTime * 4.0;
    float4 bg = background(uv, t);

    float4 fg = float4(0.0);
    int n = 5;
    if (uv.y < 0.5) {
        for (int i = 0; i < n; ++i) {
            fg += foreground(uv, t + 4.0 * float(i) / float(n) / 60.0) / float(n);
        }
    }

    float3 col = bg.rgb;
    float k;
    float h;
    float dist;
    float2 uv2;
    uv.y -= 0.2;

    // Train
    uv2 = frac(uv * 9.0);
    float wagon = 1.0;
    wagon *= 1.0 - step(0.45, uv.x);
    wagon *= 1.0 - step(0.115, uv.y);
    wagon *= step(0.103, uv.y);
    wagon *= step(0.05, 1.0 - abs(uv2.x * 2.0 - 1.0));

    float join = 1.0;
    join *= 1.0 - step(0.45, uv.x);
    join *= 1.0 - step(0.11, uv.y);
    join *= step(0.107, uv.y);

    float roof = 1.0;
    roof *= 1.0 - step(0.45, uv.x);
    roof *= 1.0 - step(0.117, uv.y);
    roof *= step(0.11, uv.y);
    roof *= step(0.15, 1.0 - abs(uv2.x * 2.0 - 1.0));

    float loco = box(uv, 0.45, 0.5, 0.103, 0.112);
    float chem1 = box(uv, 0.49, 0.495, 0.103, 0.12);
    float chem2 = box(uv, 0.488, 0.496, 0.12, 0.123);
    float locoRoof = box(uv, 0.443, 0.47, 0.11, 0.117);

    float wheel = 1.0 - step(0.00004, dot2(uv - float2(0.457, 0.106)));
    wheel += 1.0 - step(0.00002, dot2(uv - float2(0.487, 0.105)));
    wheel += 1.0 - step(0.00002, dot2(uv - float2(0.497, 0.105)));

    if (uv.x < 0.45 && uv.y > 0.025 && uv.y < 0.2) {
        wheel += 1.0 - step(0.002, dot2(uv2 - float2(0.2, 0.95)));
        wheel += 1.0 - step(0.002, dot2(uv2 - float2(0.8, 0.95)));
    }
    col = lerp(col, float3(0.18, 0.12, 0.15), join);
    col = lerp(col, float3(0.48, 0.19, 0.20), wagon);
    col = lerp(col, float3(0.18, 0.12, 0.15), roof);
    col = lerp(col, float3(0.38, 0.19, 0.20), loco);
    col = lerp(col, float3(0.38, 0.19, 0.20), chem1);
    col = lerp(col, float3(0.18, 0.12, 0.15), locoRoof);
    col = lerp(col, float3(0.18, 0.12, 0.15), chem2 + wheel);

    // Locomotive smoke
    dist = 5.0;
    uv2 = uv + float2(t / dist + 3.5, 0.0);
    uv2.x -= t / dist * 0.2;
    h = fbm2(uv2, 8) - 0.55;
    if (uv.x < 0.49) {
        float x = -uv.x + 0.49;
        float y = abs(uv.y + h * 0.4 - 0.16 * sqrt(x) - 0.12) - 0.8 * x * exp(-x * 10.0);
        if (y < 0.0) col = float3(1.0, 0.94, 0.91);
        if (y < -0.02) col = float3(0.92, 0.85, 0.82);
    }

    // Bridge
    dist = 5.0;
    uv2 = uv + float2(t / dist + 32.5, 0.0);
    uv2.x = frac(uv2.x * 3.0);
    k = 1.0;
    k *= smoothstep(0.001, 0.003, abs(uv2.y - pow(uv2.x - 0.5, 2.0) * 0.15 - 0.12));
    k *= min(step(0.05, 1.0 - abs(uv2.x * 2.0 - 1.0)) + step(0.17, uv2.y), 1.0);
    k *= min(smoothstep(0.02, 0.05, 1.0 - abs(uv2.x * 2.0 - 1.0)) + step(0.177, uv2.y), 1.0);
    k *= min(step(0.1, uv2.y) +
        smoothstep(-0.09, -0.085, -uv2.y - 0.001 / (1.0 - abs(uv2.x * 2.0 - 1.0))), 1.0);
    k *= min(smoothstep(0.05, 0.2, 1.0 - abs(frac(uv2.x * 16.0) * 2.0 - 1.0)) +
        step(0.12, uv2.y - pow(uv2.x - 0.5, 2.0) * 0.15) + step(-0.1, -uv2.y), 1.0);
    col = lerp(float3(0.29, 0.09, 0.08) * smoothstep(-0.08, 0.08, uv.y), col, k);

    col = lerp(col, fg.rgb, fg.a);
    uv = fragCoord / iResolution.xy;
    // Vulkan render targets use a top-left origin; ShaderToy buffer UVs use bottom-left.
    col = lerp(col, iChannel1.Sample(feedbackSampler, float2(uv.x, 1.0 - uv.y)).rgb, 0.3);
    return float4(col, 1.0);
}
