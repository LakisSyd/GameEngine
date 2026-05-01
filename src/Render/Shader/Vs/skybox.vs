////////////////////////////////////////////////////////////////////////////////
// Filename: skybox.vs
////////////////////////////////////////////////////////////////////////////////

cbuffer MatrixBuffer
{
    matrix worldMatrix;
    matrix viewMatrix;
    matrix projectionMatrix;
};

struct VertexInputType
{
    float4 position : POSITION;
    float2 tex : TEXCOORD0;
};

struct PixelInputType
{
    float4 position : SV_POSITION;
    float2 tex : TEXCOORD0;
};

PixelInputType SkyboxVertexShader(VertexInputType input)
{
    PixelInputType output;

    input.position.w = 1.0f;

    // Преобразуем позицию вершины
    output.position = mul(input.position, worldMatrix);
    output.position = mul(output.position, viewMatrix);
    output.position = mul(output.position, projectionMatrix);

    // Передаем UV координаты в пиксельный шейдер
    output.tex = input.tex;

    return output;
}
