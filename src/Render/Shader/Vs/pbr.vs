// ============================================================
// pbr.vs — Вершинный шейдер для PBR
// ============================================================
#define NUM_LIGHTS 4
#define MAX_BONES 128

cbuffer MatrixBuffer : register(b0)
{
    matrix worldMatrix;
    matrix viewMatrix;
    matrix projectionMatrix;
};

cbuffer BoneTransforms : register(b8)
{
      row_major matrix boneTransforms[MAX_BONES];

};


cbuffer LightPositionBuffer : register(b1)
{
    float4 lightPosition[NUM_LIGHTS];
};

#include "fog_common.hlsli"

float3 SafeNormalize(float3 value, float3 fallback)
{
    float lenSq = dot(value, value);
    return (lenSq > 0.000001f) ? value * rsqrt(lenSq) : fallback;
}

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

    float4 skinnedPos = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float3 skinnedNormal = float3(0.0f, 0.0f, 0.0f);
    float3 skinnedTangent = float3(0.0f, 0.0f, 0.0f);
    float3 skinnedBinormal = float3(0.0f, 0.0f, 0.0f);
    float totalWeight = 0.0f;
    bool hasValidBones = false;

    for (int i = 0; i < 4; i++)
    {
        int boneId = input.boneIDs[i];
        if (boneId < 0 || boneId >= MAX_BONES) continue;

        float weight = input.boneWeights[i];
        if (weight <= 0.0f) continue;

        hasValidBones = true;
        totalWeight += weight;
        matrix boneMat = boneTransforms[boneId];

        skinnedPos += mul(input.position, boneMat) * weight;

        float3x3 boneRot = (float3x3)boneMat;
        skinnedNormal += mul(input.normal, boneRot) * weight;
        skinnedTangent += mul(input.tangent, boneRot) * weight;
        skinnedBinormal += mul(input.binormal, boneRot) * weight;
    }

    // 🔥 ВАЖНО: Если нет валидных весов костей, используем оригинальные данные.
    // Не применяем boneTransforms[0] как fallback: для статичных PBR-моделей
    // слот b8 может содержать матрицы от предыдущей skinned-модели, что визуально
    // сдвигает/масштабирует меш относительно TransformComponent.
    if (hasValidBones && totalWeight > 0.0001f)
    {
        float invWeight = 1.0f / totalWeight;
        skinnedPos *= invWeight;
        skinnedNormal *= invWeight;
        skinnedTangent *= invWeight;
        skinnedBinormal *= invWeight;
    }

    float4 finalPos = hasValidBones ? skinnedPos : input.position;
    float3 finalNormal = hasValidBones ? skinnedNormal : input.normal;
    float3 finalTangent = hasValidBones ? skinnedTangent : input.tangent;
    float3 finalBinormal = hasValidBones ? skinnedBinormal : input.binormal;

    finalPos.w = 1.0f;


    float4 worldPos = mul(finalPos, worldMatrix);
    float4 viewPos  = mul(worldPos, viewMatrix);
    output.position = mul(viewPos, projectionMatrix);

    output.tex       = input.tex;
    output.worldPos  = worldPos.xyz;
    output.fogFactor = ComputeFogFactor(viewPos.z);

    float3x3 worldRot = (float3x3)worldMatrix;

    output.normal   = SafeNormalize(mul(finalNormal, worldRot), float3(0.0f, 1.0f, 0.0f));
    output.tangent  = SafeNormalize(mul(finalTangent, worldRot), float3(1.0f, 0.0f, 0.0f));
    output.binormal = SafeNormalize(mul(finalBinormal, worldRot), SafeNormalize(cross(output.normal, output.tangent), float3(0.0f, 0.0f, 1.0f)));

    for (int i = 0; i < NUM_LIGHTS; i++)
        output.lightVec[i] = SafeNormalize(lightPosition[i].xyz - worldPos.xyz, float3(0.0f, -1.0f, 0.0f));

    return output;
}
