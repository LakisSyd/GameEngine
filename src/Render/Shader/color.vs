////////////////////////////////////////////////////////////////////////////////
// Имя файла: color.vs
////////////////////////////////////////////////////////////////////////////////

/////////////
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
/////////////

// Важно: имена переменных здесь должны ТОЧНО совпадать с теми, 
// которые вы используете в функции шейдера (регистр имеет значение!).
cbuffer MatrixBuffer
{
    matrix worldMatrix;
    matrix viewMatrix;
    matrix projectionMatrix;
};

//////////////
// ОПРЕДЕЛЕНИЯ ТИПОВ
//////////////

// Входная структура (должна совпадать с Layout в C++ и VertexType в ModelClass)
struct VertexInputType
{
    float4 position : POSITION;
    float4 color : COLOR;
};

// Выходная структура (передается в пиксельный шейдер)
struct PixelInputType
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

////////////////////////////////////////////////////////////////////////////////
// ВЕРШИННЫЙ ШЕЙДЕР
////////////////////////////////////////////////////////////////////////////////

// Обратите внимание: входной параметр теперь типа VertexInputType (а не VertexType)
PixelInputType ColorVertexShader(VertexInputType input)
{
    PixelInputType output;

    // Устанавливаем w компонент в 1.0 для корректного умножения матриц
    input.position.w = 1.0f;

    // Вычисляем позицию вершины
    // Теперь имена worldMatrix, viewMatrix, projectionMatrix точно найдутся в cbuffer выше
    output.position = mul(input.position, worldMatrix);
    output.position = mul(output.position, viewMatrix);
    output.position = mul(output.position, projectionMatrix);

    // Передаем цвет дальше
    output.color = input.color;

    return output;
}