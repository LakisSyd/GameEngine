#pragma once

#include "BaseShader.h"

#define NUM_LIGHTS 4

class PBRShaderClass : public ShaderBaseClass
{
public:
    PBRShaderClass() = default;
    ~PBRShaderClass() { Shutdown(); }

    bool Initialize(ID3D11Device* device, HWND hwnd) override;
    void Shutdown() override;

    // Основной метод рендера — вызывается из RenderMesh.h
    bool Render(
        ID3D11DeviceContext* context,
        int                  indexCount,
        XMMATRIX             world,
        XMMATRIX             view,
        XMMATRIX             projection,
        ID3D11ShaderResourceView* albedo,     // t0
        ID3D11ShaderResourceView* normalMap,  // t1
        ID3D11ShaderResourceView* metalness,  // t2
        ID3D11ShaderResourceView* roughness,  // t3
        XMFLOAT4  lightColors[NUM_LIGHTS],
        XMFLOAT4  lightPositions[NUM_LIGHTS],
        XMFLOAT3  cameraPos,
        float     selectedFactor = 1.0f);

    ShaderType GetType() const override { return ShaderType::PBR; }

    // Fallback SRV'ы (если модель не содержит PBR-карты)
    ID3D11ShaderResourceView* GetDefaultNormalSRV() const { return m_defaultNormalSRV; }
    ID3D11ShaderResourceView* GetDefaultMetalnessSRV() const { return m_defaultMetalnessSRV; }
    ID3D11ShaderResourceView* GetDefaultRoughnessSRV() const { return m_defaultRoughnessSRV; }

private:
    // ── Constant buffer structs ───────────────────────────────────────────────
    // Каждый struct должен быть кратен 16 байтам (правило GPU)
    struct MatrixBuffer
    {
        XMMATRIX world;
        XMMATRIX view;
        XMMATRIX projection;
    };

    struct LightPositionBuffer
    {
        XMFLOAT4 lightPosition[NUM_LIGHTS]; // 4 × 16 = 64 байта
    };

    struct LightColorBuffer
    {
        XMFLOAT4 diffuseColor[NUM_LIGHTS];  // 4 × 16 = 64 байта
    };

    struct PBRParamsBuffer
    {
        XMFLOAT3 cameraPos; // 12 байт
        float    padding;   // +4 → итого 16 байт ✓
    };

    struct SelectedBuffer
    {
        float    selectedFactor; // 4 байта
        XMFLOAT3 padding;        // +12 → итого 16 байт ✓
    };

    // ── GPU буферы ────────────────────────────────────────────────────────────
    ID3D11Buffer* m_matrixBuffer = nullptr; // b0 VS
    ID3D11Buffer* m_lightPosBuffer = nullptr; // b1 VS+PS
    ID3D11Buffer* m_lightColorBuffer = nullptr; // b0 PS
    ID3D11Buffer* m_pbrParamsBuffer = nullptr; // b3 PS
    ID3D11Buffer* m_selectedBuffer = nullptr; // b4 PS

    // ── Default PBR textures (1x1) ───────────────────────────────────────────
    ID3D11ShaderResourceView* m_defaultNormalSRV = nullptr;    // RGBA(128,128,255,255)
    ID3D11ShaderResourceView* m_defaultMetalnessSRV = nullptr; // R=0
    ID3D11ShaderResourceView* m_defaultRoughnessSRV = nullptr; // R=1
};