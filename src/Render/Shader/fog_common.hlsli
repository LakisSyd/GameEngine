#ifndef DX_FRAMEWORK_FOG_COMMON_HLSLI
#define DX_FRAMEWORK_FOG_COMMON_HLSLI

cbuffer FogBuffer : register(b2)
{
    float4 fogColor;
    float fogStart;
    float fogEnd;
    float fogEnabled;
    float fogPadding;
};

float ComputeFogFactor(float viewDepth)
{
    if (fogEnabled < 0.5f)
        return 1.0f;

    const float fogRange = max(fogEnd - fogStart, 0.0001f);
    const float depth = abs(viewDepth);
    return saturate((fogEnd - depth) / fogRange);
}

float4 ApplyFog(float4 color, float fogFactor)
{
    if (fogEnabled < 0.5f)
        return color;

    return lerp(fogColor, color, fogFactor);
}

#endif
