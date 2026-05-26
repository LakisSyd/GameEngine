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
    if (fogEnabled < 0.5f)
        return 1.0f;

    float fogRange = max(fogEnd - fogStart, 0.0001f);
    float depth = abs(viewDepth);
    return saturate((fogEnd - depth) / fogRange);
}

// Применяет туман к финальному цвету пикселя
// fogFactor: 1.0 = нет тумана, 0.0 = полный туман
float4 ApplyFog(float4 color, float fogFactor)
{
    if (fogEnabled < 0.5f)
        return color;

    return lerp(fogColor, color, fogFactor);
}
