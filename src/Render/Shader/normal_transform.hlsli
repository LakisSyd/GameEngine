#ifndef DX_FRAMEWORK_NORMAL_TRANSFORM_HLSLI
#define DX_FRAMEWORK_NORMAL_TRANSFORM_HLSLI

float3 NormalizeDirectionOr(float3 value, float3 fallback)
{
    const float lengthSquared = dot(value, value);
    return lengthSquared > 0.000001f
        ? value * rsqrt(lengthSquared)
        : fallback;
}

float3x3 BuildNormalMatrix(float3x3 objectToWorld)
{
    // For row-vector multiplication the correct normal transform is
    // inverse(transpose(objectToWorld)). The cofactor rows below compute it
    // without requiring an additional constant buffer.
    const float3 cofactor0 = cross(objectToWorld[1], objectToWorld[2]);
    const float3 cofactor1 = cross(objectToWorld[2], objectToWorld[0]);
    const float3 cofactor2 = cross(objectToWorld[0], objectToWorld[1]);
    const float determinant = dot(objectToWorld[0], cofactor0);

    if (abs(determinant) <= 0.000001f)
        return objectToWorld;

    return float3x3(cofactor0, cofactor1, cofactor2) / determinant;
}

float3 TransformNormalToWorld(float3 localNormal, float3x3 objectToWorld)
{
    return NormalizeDirectionOr(
        mul(localNormal, BuildNormalMatrix(objectToWorld)),
        float3(0.0f, 1.0f, 0.0f));
}

void BuildWorldTangentBasis(
    float3 localNormal,
    float3 localTangent,
    float3 localBinormal,
    float3x3 objectToWorld,
    out float3 worldNormal,
    out float3 worldTangent,
    out float3 worldBinormal)
{
    worldNormal = TransformNormalToWorld(localNormal, objectToWorld);

    float3 transformedTangent = mul(localTangent, objectToWorld);
    transformedTangent -= worldNormal * dot(transformedTangent, worldNormal);
    const float3 fallbackAxis = abs(worldNormal.y) < 0.999f
        ? float3(0.0f, 1.0f, 0.0f)
        : float3(1.0f, 0.0f, 0.0f);
    worldTangent = NormalizeDirectionOr(
        transformedTangent,
        normalize(cross(fallbackAxis, worldNormal)));

    const float localHandedness = dot(
        cross(localNormal, localTangent), localBinormal) < 0.0f
        ? -1.0f
        : 1.0f;
    const float objectOrientation = determinant(objectToWorld) < 0.0f
        ? -1.0f
        : 1.0f;
    const float handedness = localHandedness * objectOrientation;
    worldBinormal = NormalizeDirectionOr(
        cross(worldNormal, worldTangent) * handedness,
        float3(0.0f, 0.0f, 1.0f));
}

#endif
