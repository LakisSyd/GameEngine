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

    // Для skybox устанавливаем w = 0, чтобы вершины не подвергались трансляции
    // Это заставляет вершины оставаться на бесконечности
    float4 positionWithW0 = float4(input.position.xyz, 0.0f);
    
    // Преобразуем позицию вершины - только вращение, без трансляции
    output.position = mul(positionWithW0, worldMatrix);
    output.position = mul(output.position, viewMatrix);
    output.position = mul(output.position, projectionMatrix);
    
    // После всех трансформаций устанавливаем w = 1 для правильной растеризации
    output.position.w = 1.0f;

    // Передаем UV координаты в пиксельный шейдер
    output.tex = input.tex;

    return output;
}
