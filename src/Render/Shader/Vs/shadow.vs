// src/Render/Shader/Vs/shadow.vs

cbuffer MatrixBuffer : register(b0)
{
    matrix worldMatrix;
    matrix lightViewProj;
};

struct VertexInput
{
    float4 position : POSITION;
};

struct PixelInput
{
    float4 position : SV_POSITION;
};

PixelInput ShadowVertexShader(VertexInput input)
{
    PixelInput output;
    
    // Трансформируем в light space
    output.position = mul(input.position, worldMatrix);
    output.position = mul(output.position, lightViewProj);
    
    return output;
}
