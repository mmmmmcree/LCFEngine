static const float3 k_cube_positions[8] = {
    float3(-1.0, -1.0, -1.0), float3(-1.0, +1.0, -1.0), float3(+1.0, +1.0, -1.0), float3(+1.0, -1.0, -1.0),
    float3(-1.0, -1.0, +1.0), float3(-1.0, +1.0, +1.0), float3(+1.0, +1.0, +1.0), float3(+1.0, -1.0, +1.0)
};

static const uint k_cube_indices[36] = {
    2, 0, 1, 2, 3, 0, //- back face
    0, 4, 5, 0, 5, 1, //- left face
    1, 5, 6, 1, 6, 2, //- right face
    2, 6, 7, 2, 7, 3, //- top face
    3, 7, 4, 3, 4, 0, //- bottom face
    4, 7, 6, 4, 6, 5 //- front face
};

// ===== 6 个面的 view 矩阵（从 .geom 拷过来，转置为行优先）=====
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
VSOutput main(uint vertex_id : SV_VertexID)
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
void main(triangle VSOutput input[3], inout TriangleStream<GSOutput> stream)
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
float4 main(GSOutput input) : SV_Target
{
    static const float PI = 3.14159265359;
    float3 n = normalize(input.uvw);
    float phi = asin(n.y);
    float theta = atan2(n.z, n.x);
    float2 uv = float2(0.5 + 0.5 * theta / PI, phi / PI + 0.5);
    return sphere_map.Sample(uv);
}