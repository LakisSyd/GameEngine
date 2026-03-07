////////////////////////////////////////////////////////////////////////////////
// Имя файла: fog.vs
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

// 👇 Новый буфер для параметров тумана
cbuffer FogBuffer
{
    float fogStart;  // Начало зоны тумана
    float fogEnd;    // Конец зоны тумана
};

//////////////
// ОПРЕДЕЛЕНИЯ ТИПОВ //
//////////////
struct VertexInputType
{
    float4 position : POSITION;
    float2 tex : TEXCOORD0;
};

// 👇 PixelInputType расширен полем fogFactor
struct PixelInputType
{
    float4 position : SV_POSITION;
    float2 tex : TEXCOORD0;
    float fogFactor : FOG;  // Фактор тумана для передачи в пиксельный шейдер
};

////////////////////////////////////////////////////////////////////////////////
// Вершинный шейдер //
////////////////////////////////////////////////////////////////////////////////
PixelInputType FogVertexShader(VertexInputType input)
{
    PixelInputType output;
    float4 cameraPosition;
    
    // Устанавливаем w=1.0 для корректных матричных преобразований
    input.position.w = 1.0f;
    
    // Вычисляем позицию вершины через матрицы мира, вида и проекции
    output.position = mul(input.position, worldMatrix);
    output.position = mul(output.position, viewMatrix);
    output.position = mul(output.position, projectionMatrix);
    
    // Сохраняем текстурные координаты
    output.tex = input.tex;
    
    // 👇 Вычисляем позицию вершины в пространстве вида
    cameraPosition = mul(input.position, worldMatrix);
    cameraPosition = mul(cameraPosition, viewMatrix);
    
    // 👇 Вычисляем линейный фактор тумана
    // saturate ограничивает результат диапазоном [0, 1]
    output.fogFactor = saturate((fogEnd - cameraPosition.z) / (fogEnd - fogStart));
    
    return output;
}