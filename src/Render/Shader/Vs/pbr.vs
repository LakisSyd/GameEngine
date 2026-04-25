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
      row_major matrix boneTransforms[128];

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

<<<<<<< HEAD
   float4 skinnedPos = float4(0.0f, 0.0f, 0.0f, 0.0f);  // ← w=0!
=======
    float4 skinnedPos = float4(0.0f, 0.0f, 0.0f, 1.0f);
>>>>>>> ccaaba2e827d646a5b57534fa6cc735f8beb8b69
    float3 skinnedNormal = float3(0.0f, 0.0f, 0.0f);
    bool hasValidBones = false;
    
    for (int i = 0; i < 4; i++)
    {
        if (input.boneIDs[i] < 0) continue;
        
        float weight = input.boneWeights[i];
        if (weight <= 0.0f) continue;
        
        hasValidBones = true;
        matrix boneMat = boneTransforms[input.boneIDs[i]];
        
        skinnedPos += mul(input.position, boneMat) * weight;
        
        float3x3 boneRot = (float3x3)boneMat;
        skinnedNormal += mul(input.normal, boneRot) * weight;
    }
    
    // 🔥 ВАЖНО: Если нет валидных костей, используем оригинальную позицию
    float4 finalPos = hasValidBones ? skinnedPos : input.position;
    float3 finalNormal = hasValidBones ? skinnedNormal : input.normal;

     finalPos.w = 1.0f; 

    
    float4 worldPos = mul(finalPos, worldMatrix);
    float4 viewPos  = mul(worldPos, viewMatrix);
    output.position = mul(viewPos, projectionMatrix);

    output.tex       = input.tex;
    output.worldPos  = worldPos.xyz;
    output.fogFactor = ComputeFogFactor(viewPos.z);

    float3x3 worldRot = (float3x3)worldMatrix;
    
    output.normal   = normalize(mul(finalNormal, worldRot));
    output.tangent  = normalize(mul(input.tangent, worldRot));
    output.binormal = normalize(mul(input.binormal, worldRot));

    for (int i = 0; i < NUM_LIGHTS; i++)
        output.lightVec[i] = normalize(lightPosition[i].xyz - worldPos.xyz);

    return output;
}