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

cbuffer LightBuffer
{
    float4 diffuseColor;
    float3 lightDirection;
    float padding;
};

//////////////
// ОПРЕДЕЛЕНИЯ ТИПОВ //
//////////////
struct VertexInputType
{
    float4 position : POSITION;
    float2 tex : TEXCOORD0;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 binormal : BINORMAL;
};

struct PixelInputType
{
    float4 position : SV_POSITION;
    float2 tex : TEXCOORD0;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 binormal : BINORMAL;
    float3 lightDirTangent : TEXCOORD1;
};

////////////////////////////////////////////////////////////////////////////////
// Вершинный шейдер //
////////////////////////////////////////////////////////////////////////////////
PixelInputType NormalMapVertexShader(VertexInputType input)
{
    PixelInputType output;
    float3 worldNormal;
    float3 worldTangent;
    float3 worldBinormal;
    float3 worldLightDir;
    
    // Устанавливаем w=1.0 для корректных матричных преобразований
    input.position.w = 1.0f;
    
    // Вычисляем позицию вершины через матрицы мира, вида и проекции
    output.position = mul(input.position, worldMatrix);
    output.position = mul(output.position, viewMatrix);
    output.position = mul(output.position, projectionMatrix);
    
    // Сохраняем текстурные координаты
    output.tex = input.tex;
    
    // Преобразуем векторы в мировое пространство и нормализуем
    worldNormal = normalize(mul(input.normal, (float3x3)worldMatrix));
    worldTangent = normalize(mul(input.tangent, (float3x3)worldMatrix));
    worldBinormal = normalize(mul(input.binormal, (float3x3)worldMatrix));
    
    // Передаём векторы TBN в пиксельный шейдер
    output.normal = worldNormal;
    output.tangent = worldTangent;
    output.binormal = worldBinormal;
    
    // Преобразуем направление света в касательное пространство
    worldLightDir = normalize(lightDirection);
    
    output.lightDirTangent.x = dot(worldLightDir, worldTangent);
    output.lightDirTangent.y = dot(worldLightDir, worldBinormal);
    output.lightDirTangent.z = dot(worldLightDir, worldNormal);
    
    return output;
}