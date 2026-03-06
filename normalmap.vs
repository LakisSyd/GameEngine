////////////////////////////////////////////////////////////////////////////////
// Имя файла: normalmap.vs
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
    float4 position : POSITION;    // Позиция вершины
    float2 tex : TEXCOORD0;         // Текстурные координаты
    float3 normal : NORMAL;         // Нормаль поверхности
    float3 tangent : TANGENT;       // Тангенс (для нормал-маппинга)
    float3 binormal : BINORMAL;     // Бинормаль (для нормал-маппинга)
};

struct PixelInputType
{
    float4 position : SV_POSITION;
    float2 tex : TEXCOORD0;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 binormal : BINORMAL;
};

////////////////////////////////////////////////////////////////////////////////
// Вершинный шейдер //
////////////////////////////////////////////////////////////////////////////////
PixelInputType NormalMapVertexShader(VertexInputType input)
{
    PixelInputType output;
    
    // Устанавливаем w=1.0 для корректных матричных преобразований
    input.position.w = 1.0f;
    
    // Вычисляем позицию вершины через матрицы мира, вида и проекции
    output.position = mul(input.position, worldMatrix);
    output.position = mul(output.position, viewMatrix);
    output.position = mul(output.position, projectionMatrix);
    
    // Сохраняем текстурные координаты для пиксельного шейдера
    output.tex = input.tex;
    
    // Вычисляем нормаль относительно матрицы мира и нормализуем
    output.normal = mul(input.normal, (float3x3)worldMatrix);
    output.normal = normalize(output.normal);
    
    // Вычисляем тангенс относительно матрицы мира и нормализуем
    output.tangent = mul(input.tangent, (float3x3)worldMatrix);
    output.tangent = normalize(output.tangent);
    
    // Вычисляем бинормаль относительно матрицы мира и нормализуем
    output.binormal = mul(input.binormal, (float3x3)worldMatrix);
    output.binormal = normalize(output.binormal);
    
    return output;
}