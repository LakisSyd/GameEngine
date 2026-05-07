// SkyShader.vs
struct VertexInput {
    float3 position : POSITION;
};

struct PixelInput {  // 🔥 Переименовал, чтобы не путать с .ps
    float4 positionClip : SV_POSITION;
    float3 worldPos : TEXCOORD0;
};

cbuffer SkyBuffer : register(b0)
{
    matrix modelViewProjection;
};

// 🔥 Функция должна называться SkyVS (как в CompileShader)
PixelInput SkyVS(VertexInput input)
{
    PixelInput output;
    
    // 1. Позиция (как было)
    output.positionClip = mul(float4(input.position, 1.0f), modelViewProjection).xyww;
    
    // 2. 🔥 ИСПРАВЛЕНИЕ: Нормализуем вектор!
    output.worldPos = normalize(input.position); 
    
    return output;
}