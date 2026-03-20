// ============================================================
// pbr.vs — Вершинный шейдер для PBR
// ============================================================
#define NUM_LIGHTS 4

cbuffer MatrixBuffer : register(b0)
{
    matrix worldMatrix;
    matrix viewMatrix;
    matrix projectionMatrix;
};

cbuffer LightPositionBuffer : register(b1)
{
    float4 lightPosition[NUM_LIGHTS];
};

#include "fog_common.hlsli"

struct VertexInputType
{
    float4 position : POSITION;
    float2 tex      : TEXCOORD0;
    float3 normal   : NORMAL;
    float3 tangent  : TANGENT;
    float3 binormal : BINORMAL;
};

struct PixelInputType
{
    float4 position              : SV_POSITION;
    float2 tex                   : TEXCOORD0;
    float3 normal                : NORMAL;
    float3 tangent               : TANGENT;
    float3 binormal              : BINORMAL;
    float3 worldPos              : TEXCOORD5;
    float3 lightVec[NUM_LIGHTS]  : TEXCOORD6;
    float  fogFactor             : FOG;
};

PixelInputType PBRVertexShader(VertexInputType input)
{
    PixelInputType output;

    input.position.w = 1.0f;

    float4 worldPos  = mul(input.position, worldMatrix);
    float4 viewPos   = mul(worldPos,        viewMatrix);
    output.position  = mul(viewPos,         projectionMatrix);

    output.tex       = input.tex;
    output.worldPos  = worldPos.xyz;
    output.fogFactor = ComputeFogFactor(viewPos.z);

    float3x3 worldRot = (float3x3)worldMatrix;

    // ❌ Убрали отрицания — теперь как в Light-шейдере
    output.normal   = normalize(mul(input.normal,   worldRot));
    output.tangent  = normalize(mul(input.tangent,  worldRot));
    output.binormal = normalize(mul(input.binormal, worldRot));

    for (int i = 0; i < NUM_LIGHTS; i++)
        output.lightVec[i] = normalize(lightPosition[i].xyz - worldPos.xyz);

    return output;
}