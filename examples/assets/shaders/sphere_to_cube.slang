#include "constant_data/cube.slang"

static const float4x4 k_view[6] = {
    { 0.0, 0.0, -1.0, 0.0,   0.0, -1.0, 0.0, 0.0,   -1.0, 0.0, 0.0, 0.0,   0.0, 0.0, 0.0, 1.0 },
    { 0.0, 0.0, 1.0, 0.0,    0.0, -1.0, 0.0, 0.0,    1.0, 0.0, 0.0, 0.0,   0.0, 0.0, 0.0, 1.0 },
    { 1.0, 0.0, 0.0, 0.0,    0.0, 0.0, -1.0, 0.0,    0.0, 1.0, 0.0, 0.0,   0.0, 0.0, 0.0, 1.0 },
    { 1.0, 0.0, 0.0, 0.0,    0.0, 0.0, 1.0, 0.0,     0.0, -1.0, 0.0, 0.0,  0.0, 0.0, 0.0, 1.0 },
    { 1.0, 0.0, 0.0, 0.0,    0.0, -1.0, 0.0, 0.0,    0.0, 0.0, -1.0, 0.0,  0.0, 0.0, 0.0, 1.0 },
    { -1.0, 0.0, 0.0, 0.0,   0.0, -1.0, 0.0, 0.0,    0.0, 0.0, 1.0, 0.0,   0.0, 0.0, 0.0, 1.0 }
};

static const float4x4 k_projection = { 1.0, 0.0, 0.0, 0.0,   0.0, 1.0, 0.0, 0.0,   0.0, 0.0, -1.0, -0.2,   0.0, 0.0, -1.0, 0.0 };

// ===== 顶点输出（也是 GS 的输入） =====
struct VSOutput {
    float3 uvw : VARYING0;
    float4 sv_position : SV_Position;
};

// ===== GS 输出 =====
struct GSOutput {
    float3 uvw : VARYING0;
    float4 sv_position : SV_Position;
    uint   sv_layer : SV_RenderTargetArrayIndex;
};

// ===== Vertex Shader =====
[shader("vertex")]
VSOutput vs_main(uint vertex_id : SV_VertexID)
{
    uint idx = k_cube_indices[vertex_id];
    float3 pos = k_cube_positions[idx];
    VSOutput o;
    o.uvw = pos;
    o.sv_position = float4(pos, 1.0);
    return o;
}

// ===== Geometry Shader =====
[shader("geometry")]
[maxvertexcount(18)]
void gs_main(triangle VSOutput input[3], inout TriangleStream<GSOutput> stream)
{
    for (uint face = 0; face < 6; ++face) {
        for (uint i = 0; i < 3; ++i) {
            GSOutput o;
            o.sv_position = mul(mul(k_projection, k_view[face]), input[i].sv_position);
            o.uvw = input[i].uvw;
            o.sv_layer = face;
            stream.Append(o);
        }
        stream.RestartStrip();
    }
}

// ===== Sampler 资源 =====
[vk::binding(0, 0)] Sampler2D sphere_map;
 
// ===== Fragment Shader =====
[shader("fragment")]
float4 fs_main(GSOutput input) : SV_Target
{
    static const float PI = 3.14159265359;
    float3 n = normalize(input.uvw);
    float phi = asin(n.y);
    float theta = atan2(n.z, n.x);
    float2 uv = float2(0.5 + 0.5 * theta / PI, phi / PI + 0.5);
    return sphere_map.Sample(uv);
}