#include "common/fullscreen.slangh"
#include "common/ShaderToyParams.slangh"

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
