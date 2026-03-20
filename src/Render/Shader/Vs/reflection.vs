cbuffer MatrixBuffer : register(b0)
{
    matrix worldMatrix;
    matrix viewMatrix;
    matrix projectionMatrix;
};

cbuffer ReflectionBuffer : register(b1)
{
    matrix reflectionMatrix;
};

struct VertexInputType
{
    float4 position : POSITION;
    float2 tex      : TEXCOORD0;
    float3 normal   : NORMAL;
};

struct PixelInputType
{
    float4 position    : SV_POSITION;
    float2 tex         : TEXCOORD0;
    float4 reflectPos  : TEXCOORD1;
};

PixelInputType ReflectionVertexShader(VertexInputType input)
{
    PixelInputType output;

    input.position.w = 1.0f;

    output.position   = mul(input.position, worldMatrix);
    output.position   = mul(output.position, viewMatrix);
    output.position   = mul(output.position, projectionMatrix);

    output.tex = input.tex;

    // ѕозици€ в пространстве отражени€ (дл€ UV-проецировани€)
    output.reflectPos = mul(input.position, worldMatrix);
    output.reflectPos = mul(output.reflectPos, reflectionMatrix);
    output.reflectPos = mul(output.reflectPos, projectionMatrix);

    return output;
}