////////////////////////////////////////////////////////////////////////////////
// Water.vs — по туториалу 31 (rastertek.com)
////////////////////////////////////////////////////////////////////////////////

cbuffer MatrixBuffer : register(b0)
{
    matrix worldMatrix;
    matrix viewMatrix;
    matrix projectionMatrix;
};

// Матрица отражённой камеры — строится как BuildReflectionMatrix()
cbuffer ReflectionBuffer : register(b1)
{
    matrix reflectionMatrix;
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
    float4 reflectionPosition : TEXCOORD1;
    float4 refractionPosition : TEXCOORD2;
};

PixelInputType WaterVertexShader(VertexInputType input)
{
    PixelInputType output;
    matrix reflectProjectWorld;
    matrix viewProjectWorld;

    input.position.w = 1.0f;

    // Стандартное преобразование позиции
    output.position = mul(input.position, worldMatrix);
    output.position = mul(output.position, viewMatrix);
    output.position = mul(output.position, projectionMatrix);

    // UV для нормал-карты — PS сдвинет input.tex.y на waterTranslation
    output.tex = input.tex;

    // Проективные координаты отражения: reflection * projection * world
    reflectProjectWorld = mul(reflectionMatrix, projectionMatrix);
    reflectProjectWorld = mul(worldMatrix, reflectProjectWorld);
    output.reflectionPosition = mul(input.position, reflectProjectWorld);

    // Проективные координаты преломления: view * projection * world
    viewProjectWorld = mul(viewMatrix, projectionMatrix);
    viewProjectWorld = mul(worldMatrix, viewProjectWorld);
    output.refractionPosition = mul(input.position, viewProjectWorld);

    return output;
}