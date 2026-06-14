cbuffer MatrixBuffer : register(b0)
{
    matrix worldMatrix;
    matrix viewMatrix;
    matrix projectionMatrix;
};

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
    float4 position : SV_POSITION;
    float3 worldPos : TEXCOORD0;
    float2 maskUV   : TEXCOORD1;
    float3 tangent  : TEXCOORD2;
    float3 binormal : TEXCOORD3;
    float3 normal   : NORMAL;
};

PixelInputType TerrainBlendVertexShader(VertexInputType input)
{
    PixelInputType output;
    input.position.w = 1.0f;

    float4 worldPos = mul(input.position, worldMatrix);
    float4 viewPos = mul(worldPos, viewMatrix);
    output.position = mul(viewPos, projectionMatrix);

    output.worldPos = worldPos.xyz;
    output.maskUV = input.tex;
    output.normal = normalize(mul(input.normal, (float3x3)worldMatrix));
    output.tangent = normalize(mul(input.tangent, (float3x3)worldMatrix));
    output.binormal = normalize(mul(input.binormal, (float3x3)worldMatrix));
    return output;
}
