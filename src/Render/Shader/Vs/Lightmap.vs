////////////////////////////////////////////////////////////////////////////////
// Имя файла: lightmap.vs
////////////////////////////////////////////////////////////////////////////////
/////////////
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ //
/////////////
cbuffer MatrixBuffer
{
    matrix worldMatrix;
    matrix viewMatrix;
    matrix projectionMatrix;
};

//////////////
// ОПРЕДЕЛЕНИЯ ТИПОВ //
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
// Вершинный шейдер //
////////////////////////////////////////////////////////////////////////////////
PixelInputType LightMapVertexShader(VertexInputType input)
{
    PixelInputType output;
    
    // Изменяем вектор позиции на 4 компонента для корректных матричных вычислений.
    input.position.w = 1.0f;
    
    // Вычисляем позицию вершины относительно матриц мира, вида и проекции.
    output.position = mul(input.position, worldMatrix);
    output.position = mul(output.position, viewMatrix);
    output.position = mul(output.position, projectionMatrix);
    
    // Сохраняем текстурные координаты для пиксельного шейдера.
    output.tex = input.tex;
    
    return output;
}