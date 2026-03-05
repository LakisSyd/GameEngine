/////////////
// ОПРЕДЕЛЕНИЯ
/////////////
#define NUM_LIGHTS 4

/////////////
// ГЛОБАЛЬНЫЕ ПАРАМЕТРЫ
/////////////
cbuffer MatrixBuffer : register(b0)
{
    matrix worldMatrix;
    matrix viewMatrix;
    matrix projectionMatrix;
};

// Позиции ламп (Слот 1, так как в C++ мы поставили bufferNumber = 1)
cbuffer LightPositionBuffer : register(b1)
{
    float4 lightPosition[NUM_LIGHTS];
};

/////////////
// TYPEDEFS
/////////////
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
    float3 normal : NORMAL;
    // Передаем в пиксельный шейдер массив направлений к лампам
    float3 lightVec[NUM_LIGHTS] : TEXCOORD1; 
};

/////////////
// ВЕРШИННЫЙ ШЕЙДЕР
/////////////
PixelInputType LightVertexShader(VertexInputType input)
{
    PixelInputType output;
    float4 worldPosition;
    int i;

    input.position.w = 1.0f;
    
    // Стандартные преобразования
    worldPosition = mul(input.position, worldMatrix);
    output.position = mul(worldPosition, viewMatrix);
    output.position = mul(output.position, projectionMatrix);
    
    output.tex = input.tex;
    
    // Нормаль в мировом пространстве
    output.normal = mul(input.normal, (float3x3)worldMatrix);
    output.normal = normalize(output.normal);

    // 🔥 ГЛАВНОЕ: Считаем вектор от вершины к каждой лампе
    for(i = 0; i < NUM_LIGHTS; i++)
    {
        // Вектор = ПозицияЛампы - ПозицияВершины
        float3 vec = lightPosition[i].xyz - worldPosition.xyz;
        // Нормализуем его (делаем единичным)
        output.lightVec[i] = normalize(vec);
    }

    return output;
}