// fog_common.hlsli
// Подключать во все .vs и .ps файлы которые поддерживают туман
// #include "fog_common.hlsli"

// Слот b2 зарезервирован для тумана во ВСЕХ шейдерах
cbuffer FogBuffer : register(b2)
{
    float4 fogColor;    // rgb = цвет тумана
    float  fogStart;    // расстояние начала тумана
    float  fogEnd;      // расстояние полного тумана
    float  fogEnabled;  // 1.0 = включён, 0.0 = выключен
    float  fogPadding;
};

// Вычисляет fogFactor по глубине вершины в view-space
// viewDepth = position.z ПОСЛЕ умножения на viewMatrix, НО ДО projectionMatrix
float ComputeFogFactor(float viewDepth)
{
    return saturate((fogEnd - viewDepth) / (fogEnd - fogStart));
}

// Применяет туман к финальному цвету пикселя
// fogFactor: 1.0 = нет тумана, 0.0 = полный туман
float4 ApplyFog(float4 color, float fogFactor)
{
    return lerp(fogColor, color, fogFactor);
}
