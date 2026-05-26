// SkyShader.vs
struct PixelInput
{
    float4 positionClip : SV_POSITION;
    float3 worldPos : TEXCOORD0;
};

cbuffer SkyBuffer : register(b0)
{
    matrix invViewProjection;
};

PixelInput SkyVS(uint vertexId : SV_VertexID)
{
    float2 positions[3] =
    {
        float2(-1.0f, -1.0f),
        float2(-1.0f,  3.0f),
        float2( 3.0f, -1.0f)
    };

    float2 clip = positions[vertexId];

    PixelInput output;
    output.positionClip = float4(clip, 1.0f, 1.0f);

    float4 worldFar = mul(float4(clip, 1.0f, 1.0f), invViewProjection);
    output.worldPos = worldFar.xyz / max(abs(worldFar.w), 0.000001f);

    return output;
}
