// color.vs — ÂÅĞØÈÍÍÛÉ ØÅÉÄÅĞ

//////////////
// ÊÎÍÑÒÀÍÒÍÛÅ ÁÓÔÅĞÛ
//////////////
cbuffer MatrixBuffer : register(b0)
{
    matrix worldMatrix;
    matrix viewMatrix;
    matrix projectionMatrix;
};

//////////////
// ÒÈÏÛ ÄÀÍÍÛÕ
//////////////
struct VertexInputType
{
    float4 position : POSITION;
    float4 color : COLOR;
};

struct PixelInputType
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

////////////////////////////////////////////////////////////////////////////////
// ÂÅĞØÈÍÍÛÉ ØÅÉÄÅĞ
////////////////////////////////////////////////////////////////////////////////
PixelInputType ColorVertexShader(VertexInputType input)
{
    PixelInputType output;

    input.position.w = 1.0f;

    output.position = mul(input.position, worldMatrix);
    output.position = mul(output.position, viewMatrix);
    output.position = mul(output.position, projectionMatrix);

    output.color = input.color; // Ïåğåäà¸ì öâåò (õîòÿ â PS îí èãíîğèğóåòñÿ)

    return output;
}