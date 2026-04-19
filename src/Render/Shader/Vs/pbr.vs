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

cbuffer BoneTransforms : register(b8)
{
    matrix boneTransforms[128];
};


cbuffer LightPositionBuffer : register(b1)
{
    float4 lightPosition[NUM_LIGHTS];
};

#include "fog_common.hlsli"

// ── Обновляем входную структуру ─────────────────────────────────────
struct VertexInputType
{
    float4 position  : POSITION;
    float2 tex       : TEXCOORD0;
    float3 normal    : NORMAL;
    float3 tangent   : TANGENT;
    float3 binormal  : BINORMAL;
    
    //  НОВОЕ: Веса костей (для skinning)
    // Используем TEXCOORD1-2, чтобы не конфликтовать с другими данными
    int4   boneIDs   : TEXCOORD1;   
    float4 boneWeights : TEXCOORD2;
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

    //  SKINNING: Взвешенная трансформация вершины костями
    float4 skinnedPos = float4(0.0f, 0.0f, 0.0f, 1.0f);
    float3 skinnedNormal = float3(0.0f, 0.0f, 0.0f);
    
    for (int i = 0; i < 4; i++)
    {
        // Пропускаем невалидные индексы костей
        if (input.boneIDs[i] < 0) continue;
        
        float weight = input.boneWeights[i];
        matrix boneMat = boneTransforms[input.boneIDs[i]];
        
        // Трансформируем позицию
        skinnedPos += mul(input.position, boneMat) * weight;
        
        // Трансформируем нормаль (только вращение, без трансляции)
        float3x3 boneRot = (float3x3)boneMat;
        skinnedNormal += mul(input.normal, boneRot) * weight;
    }
    
    // Используем skinnedPos вместо input.position для дальнейших расчётов
    float4 worldPos = mul(skinnedPos, worldMatrix);
    float4 viewPos  = mul(worldPos, viewMatrix);
    output.position = mul(viewPos, projectionMatrix);

    output.tex       = input.tex;
    output.worldPos  = worldPos.xyz;
    output.fogFactor = ComputeFogFactor(viewPos.z);

    float3x3 worldRot = (float3x3)worldMatrix;
    
    // Используем skinnedNormal для освещения
    output.normal   = normalize(mul(skinnedNormal, worldRot));
    output.tangent  = normalize(mul(input.tangent, worldRot)); // tangent обычно не скиннится для простоты
    output.binormal = normalize(mul(input.binormal, worldRot));

    for (int i = 0; i < NUM_LIGHTS; i++)
        output.lightVec[i] = normalize(lightPosition[i].xyz - worldPos.xyz);

    return output;
}