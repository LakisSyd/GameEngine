////////////////////////////////////////////////////////////////////////////////
// Filename: multitexture.vs
////////////////////////////////////////////////////////////////////////////////

/////////////
// GLOBALS //
/////////////
cbuffer MatrixBuffer
{
    matrix worldMatrix;
    matrix viewMatrix;
    matrix projectionMatrix;
};

//////////////
// TYPEDEFS //
//////////////
struct VertexInputType
{
    float4 position : POSITION;
    float2 tex : TEXCOORD0;
    float3 normal : NORMAL;
};

struct PixelInputType
{
    float4 position : SV_POSITION;
    float2 tex : TEXCOORD0;
};

////////////////////////////////////////////////////////////////////////////////
// Vertex Shader
////////////////////////////////////////////////////////////////////////////////
PixelInputType MultiTextureVertexShader(VertexInputType input)
{
    PixelInputType output;

    // Изменяем вектор положения на 4 компонента для корректных вычислений с матрицами.
    input.position.w = 1.0f;

    // Рассчитываем положение вершины относительно матриц мира, вида и проекции.
    output.position = mul(input.position, worldMatrix);
    output.position = mul(output.position, viewMatrix);
    output.position = mul(output.position, projectionMatrix);

    // Сохраняем координаты текстуры для пиксельного шейдера.
    output.tex = input.tex;

    return output;
}