/////////////
// ОПРЕДЕЛЕНИЯ
/////////////
#define NUM_LIGHTS 4

/////////////
// ГЛОБАЛЬНЫЕ ПАРАМЕТРЫ
/////////////
cbuffer MatrixBuffer : register(b0)
{
    matrix worldMatrix;
    matrix viewMatrix;
    matrix projectionMatrix;
};

// Позиции ламп (Слот 1)
cbuffer LightPositionBuffer : register(b1)
{
    float4 lightPosition[NUM_LIGHTS];
};

// ✅ Туман — слот b2
#include "fog_common.hlsli"

/////////////
// TYPEDEFS
/////////////
struct VertexInputType
{
    float4 position : POSITION;
    float2 tex : TEXCOORD0;
    float3 normal : NORMAL;
};

struct PixelInputType
{
    float4 position : SV_POSITION;
    float2 tex : TEXCOORD0;
    float3 normal : NORMAL;
    float3 worldPos : TEXCOORD1;        // 🔥 ДОБАВЛЕНО: мировая позиция
    float4 shadowPos : TEXCOORD2;
    float  fogFactor : FOG;
};

/////////////
// ВЕРШИННЫЙ ШЕЙДЕР
/////////////
PixelInputType LightVertexShader(VertexInputType input)
{
    PixelInputType output;
    float4 worldPosition;

    input.position.w = 1.0f;
    
    worldPosition = mul(input.position, worldMatrix);
    output.worldPos = worldPosition.xyz;

    float4 viewPosition = mul(worldPosition, viewMatrix);
    output.fogFactor = ComputeFogFactor(viewPosition.z);

    output.position = mul(viewPosition, projectionMatrix);
    
    // 🔥 ДОБАВИТЬ: трансформ в light space (если нужно)
    // output.shadowPos = mul(worldPosition, lightViewProj);
    
    output.tex = input.tex;
    output.normal = mul(input.normal, (float3x3)worldMatrix);
    output.normal = normalize(output.normal);

    return output;
}
