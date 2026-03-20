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
};

struct PixelInputType
{
    float4 position           : SV_POSITION;
    float2 tex                : TEXCOORD0;
    float4 refractionPosition : TEXCOORD1;
};

PixelInputType GlassVertexShader(VertexInputType input)
{
    PixelInputType output;
    input.position.w = 1.0f;

    // Финальная clip-space позиция
    float4 worldPos = mul(input.position, worldMatrix);
    float4 viewPos  = mul(worldPos, viewMatrix);
    output.position = mul(viewPos, projectionMatrix);

    output.tex = input.tex;

    // refractionPosition — проекционные координаты для сэмплирования
    // refraction-текстуры (сцена за стеклом).
    // Используем ту же clip-space позицию что и output.position.
    output.refractionPosition = output.position;

    return output;
}