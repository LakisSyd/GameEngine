#pragma once

#include <array>
#include <d3d11.h>
#include <DirectXMath.h>
#include <vector>

#include "assimpmodelclass.h"
#include "Components.h"
#include "ComponentManager.h"
class GrassRenderer
{
public:
    struct GrassInstance
    {
        DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
        float rotationY = 0.0f;
        float scale = 1.0f;
    };

    GrassRenderer() = default;
    ~GrassRenderer();

    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context);
    void Shutdown();

    void Render(
        ID3D11DeviceContext* context,
        const DirectX::XMMATRIX& view,
        const DirectX::XMMATRIX& projection,
        const DirectX::XMFLOAT3& cameraPosition,
        float deltaTime,
        const DirectX::XMFLOAT3& sunDirection,
        float ambientStrength);

    void GenerateAreas(ID3D11Device* device,
        ComponentManager<WorldAreaComponent>& areas);

    void AppendArea(const WorldAreaComponent& area);

    void GenerateArea(ID3D11Device* device, const WorldAreaComponent& area);

private:
    struct GrassInstanceGpu
    {
        DirectX::XMFLOAT4 row0;
        DirectX::XMFLOAT4 row1;
        DirectX::XMFLOAT4 row2;
        DirectX::XMFLOAT4 row3;
        DirectX::XMFLOAT4 params;
    };

    struct MatrixBufferType
    {
        DirectX::XMMATRIX view;
        DirectX::XMMATRIX projection;
    };

    struct LightBufferType
    {
        DirectX::XMFLOAT4 direction;
        DirectX::XMFLOAT4 ambient;
    };

    struct GrassSettingsBufferType
    {
        DirectX::XMFLOAT4 cameraPositionAndTime;
        DirectX::XMFLOAT4 distancesAndWind;
    };

    struct BatchBufferType
    {
        DirectX::XMFLOAT4 heightData;
    };

    struct GrassBatch
    {
        AssimpModelClass model;
        std::vector<GrassInstance> instances;
        ID3D11Buffer* instanceBuffer = nullptr;
        float importedMinY = 0.0f;
        float importedHeight = 1.0f;
        float normalizationScale = 1.0f;
        UINT visibleCount = 0;
    };

    bool InitializeShaders(ID3D11Device* device);
    bool InitializeBatch(
        ID3D11Device* device,
        ID3D11DeviceContext* context,
        GrassBatch& batch,
        const char* modelPath);
    bool CreateInstanceBuffer(ID3D11Device* device, GrassBatch& batch);
    bool UpdateVisibleInstances(
        ID3D11DeviceContext* context,
        GrassBatch& batch,
        const DirectX::XMFLOAT3& cameraPosition);
    bool UpdateFrameBuffers(
        ID3D11DeviceContext* context,
        const DirectX::XMMATRIX& view,
        const DirectX::XMMATRIX& projection,
        const DirectX::XMFLOAT3& cameraPosition,
        const DirectX::XMFLOAT3& sunDirection,
        float ambientStrength);
    void RenderBatch(ID3D11DeviceContext* context, const GrassBatch& batch);
    bool TryProjectToTerrain(
        float x,
        float z,
        const WorldAreaComponent& area,
        float& terrainY) const;



    std::array<GrassBatch, 2> m_batches;
    float m_elapsedTime = 0.0f;
    float m_targetGrassHeight = 0.75f;
    float m_targetGrassFootprint = 1.40f;
    float m_cullDistance = 42.0f;
    float m_fadeStartDistance = 30.0f;
    float m_windStrength = 0.0f;
    ID3D11VertexShader* m_vertexShader = nullptr;
    ID3D11PixelShader* m_pixelShader = nullptr;
    ID3D11InputLayout* m_inputLayout = nullptr;
    ID3D11Buffer* m_matrixBuffer = nullptr;
    ID3D11Buffer* m_lightBuffer = nullptr;
    ID3D11Buffer* m_grassSettingsBuffer = nullptr;
    ID3D11Buffer* m_batchBuffer = nullptr;
    ID3D11SamplerState* m_sampler = nullptr;
    ID3D11RasterizerState* m_rasterizerState = nullptr;
};
