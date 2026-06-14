cbuffer MatrixBuffer : register(b0)
{
    matrix viewMatrix;
    matrix projectionMatrix;
};

cbuffer GrassSettingsBuffer : register(b1)
{
    float4 cameraPositionAndTime;
    float4 distancesAndWind;
};

cbuffer BatchBuffer : register(b2)
{
    float4 heightData;
};

struct VertexInputType
{
    float4 position : POSITION;
    float2 tex : TEXCOORD0;
    float3 normal : NORMAL;
    float4 instanceRow0 : INSTANCE0;
    float4 instanceRow1 : INSTANCE1;
    float4 instanceRow2 : INSTANCE2;
    float4 instanceRow3 : INSTANCE3;
    float4 instanceParams : INSTANCE4;
};

struct PixelInputType
{
    float4 position : SV_POSITION;
    float2 tex : TEXCOORD0;
    float3 normal : NORMAL;
    float distanceToCamera : TEXCOORD1;
};

PixelInputType GrassVertexShader(VertexInputType input)
{
    PixelInputType output;
    matrix worldMatrix = matrix(
        input.instanceRow0,
        input.instanceRow1,
        input.instanceRow2,
        input.instanceRow3);

    float heightFactor = saturate((input.position.y - heightData.x) * heightData.y);
    float phase =
        cameraPositionAndTime.w * 1.8f +
        input.instanceParams.x +
        input.position.x * 0.35f +
        input.position.z * 0.22f;
    float windOffset = sin(phase) * distancesAndWind.z * heightFactor * heightFactor;

    float3 bentPosition = input.position.xyz;
    bentPosition.x += windOffset;
    bentPosition.z += windOffset * 0.35f;

    float4 worldPosition = mul(float4(bentPosition, 1.0f), worldMatrix);
    output.position = mul(mul(worldPosition, viewMatrix), projectionMatrix);
    output.tex = input.tex;
    output.normal = normalize(mul(input.normal, (float3x3)worldMatrix));
    output.distanceToCamera = distance(worldPosition.xyz, cameraPositionAndTime.xyz);
    return output;
}
