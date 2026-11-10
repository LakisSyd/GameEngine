cbuffer MatrixBuffer : register(b0)
{
    matrix viewMatrix;
    matrix projectionMatrix;
};

struct VertexInput
{
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
    float2 parameters : TEXCOORD1;
};

struct PixelInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
    float2 parameters : TEXCOORD1;
};

PixelInput ParticleVertexShader(VertexInput input)
{
    PixelInput output;
    output.position = mul(float4(input.position, 1.0f), viewMatrix);
    output.position = mul(output.position, projectionMatrix);
    output.uv = input.uv;
    output.color = input.color;
    output.parameters = input.parameters;
    return output;
}
