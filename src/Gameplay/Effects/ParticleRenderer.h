#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <d3d11.h>
#include <DirectXMath.h>

#include "ParticleSystem.h"

class ParticleRenderer
{
public:
    ParticleRenderer() = default;
    ~ParticleRenderer();

    ParticleRenderer(const ParticleRenderer&) = delete;
    ParticleRenderer& operator=(const ParticleRenderer&) = delete;

    bool Initialize(ID3D11Device* device);
    void Shutdown();
    void ClearTextureCache();

    // Рендер выполняется после непрозрачной геометрии: Z-тест остаётся включён,
    // но запись глубины отключается, чтобы прозрачные частицы не резали друг друга.
    void Render(
        ID3D11DeviceContext* context,
        const DirectX::XMMATRIX& view,
        const DirectX::XMMATRIX& projection,
        const DirectX::XMFLOAT3& cameraPosition,
        const std::vector<ParticleRenderItem>& particles);

    size_t GetLastRenderedParticleCount() const { return m_lastRenderedParticleCount; }
    int GetLastDrawCallCount() const { return m_lastDrawCallCount; }

private:
    struct Vertex
    {
        DirectX::XMFLOAT3 position{};
        DirectX::XMFLOAT2 uv{};
        DirectX::XMFLOAT4 color{};
        DirectX::XMFLOAT2 parameters{}; // x = возраст 0..1, y = ParticleShape
    };

    struct MatrixBuffer
    {
        DirectX::XMMATRIX view;
        DirectX::XMMATRIX projection;
    };

    bool CreateShaders(ID3D11Device* device);
    bool CreatePipelineStates(ID3D11Device* device);
    bool RenderBatch(
        ID3D11DeviceContext* context,
        const std::vector<const ParticleRenderItem*>& particles,
        const DirectX::XMFLOAT3& cameraRight,
        const DirectX::XMFLOAT3& cameraUp,
        ID3D11BlendState* blendState,
        const std::string& texturePath);
    void RenderMaterialBatches(
        ID3D11DeviceContext* context,
        const std::vector<const ParticleRenderItem*>& particles,
        const DirectX::XMFLOAT3& cameraRight,
        const DirectX::XMFLOAT3& cameraUp,
        ID3D11BlendState* blendState);
    ID3D11ShaderResourceView* ResolveTexture(
        ID3D11DeviceContext* context,
        const std::string& texturePath);
    void AppendBillboard(
        const ParticleRenderItem& particle,
        const DirectX::XMFLOAT3& cameraRight,
        const DirectX::XMFLOAT3& cameraUp);

    ID3D11VertexShader* m_vertexShader = nullptr;
    ID3D11PixelShader* m_pixelShader = nullptr;
    ID3D11InputLayout* m_inputLayout = nullptr;
    ID3D11Buffer* m_vertexBuffer = nullptr;
    ID3D11Buffer* m_matrixBuffer = nullptr;
    ID3D11BlendState* m_alphaBlendState = nullptr;
    ID3D11BlendState* m_additiveBlendState = nullptr;
    ID3D11DepthStencilState* m_depthReadState = nullptr;
    ID3D11RasterizerState* m_rasterizerState = nullptr;
    ID3D11SamplerState* m_textureSampler = nullptr;

    std::unordered_map<std::string, ID3D11ShaderResourceView*> m_textureCache;
    std::unordered_set<std::string> m_failedTexturePaths;

    std::vector<Vertex> m_cpuVertices;
    size_t m_lastRenderedParticleCount = 0;
    int m_lastDrawCallCount = 0;
};
