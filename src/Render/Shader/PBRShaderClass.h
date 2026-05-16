#pragma once
#include "BaseShader.h"
#include "Components.h"

#define NUM_LIGHTS 4

class PBRShaderClass : public ShaderBaseClass
{
public:
    PBRShaderClass() = default;
    ~PBRShaderClass() { Shutdown(); }

    bool Initialize(ID3D11Device* device, HWND hwnd) override;
    void Shutdown() override;

    bool Render(
        ID3D11DeviceContext* context, int indexCount,
        XMMATRIX world, XMMATRIX view, XMMATRIX projection,
        ID3D11ShaderResourceView* albedo,
        ID3D11ShaderResourceView* normalMap,
        ID3D11ShaderResourceView* metalness,
        ID3D11ShaderResourceView* roughness,
        XMFLOAT4 lightColors[NUM_LIGHTS],
        XMFLOAT4 lightPositions[NUM_LIGHTS],
        XMFLOAT3 cameraPos,
        float selectedFactor = 1.0f,
        float glassOpacity = 1.0f);

    bool SetDirectionalLight(ID3D11DeviceContext* context,
        const XMFLOAT3& direction, const XMFLOAT4& color,
        const XMFLOAT4& ambient, float intensity);

    bool SetShadowMap(ID3D11DeviceContext* context,
        ID3D11ShaderResourceView* shadowMapSRV,
        const XMMATRIX& lightViewProj);

    bool SetDamageDecals(ID3D11DeviceContext* context,
        const DamageDecalComponent* damage,
        const XMMATRIX& worldMatrix);

    // Устанавливается один раз из ApplicationClass после загрузки текстуры
    void SetCrackTexture(ID3D11ShaderResourceView* srv) { m_crackTextureSRV = srv; }
    void SetEnvironmentMap(ID3D11ShaderResourceView* srv) { m_environmentMapSRV = srv; }

    // Метод для установки матриц (используется при анимации)
    void SetMatrixBuffers(ID3D11DeviceContext* context, XMMATRIX world, XMMATRIX view, XMMATRIX projection);

    ShaderType GetType() const override { return ShaderType::PBR; }

    ID3D11ShaderResourceView* GetDefaultNormalSRV()    const { return m_defaultNormalSRV; }
    ID3D11ShaderResourceView* GetDefaultMetalnessSRV() const { return m_defaultMetalnessSRV; }
    ID3D11ShaderResourceView* GetDefaultRoughnessSRV() const { return m_defaultRoughnessSRV; }

private:
    struct MatrixBuffer { XMMATRIX world, view, projection; };
    struct LightPositionBuffer { XMFLOAT4 lightPosition[NUM_LIGHTS]; };
    struct LightColorBuffer { XMFLOAT4 diffuseColor[NUM_LIGHTS]; };
    struct PBRParamsBuffer { XMFLOAT3 cameraPos; float glassOpacity; };
    struct SelectedBuffer { float selectedFactor; XMFLOAT3 padding; };
    struct ShadowBufferType { XMMATRIX lightViewProj; };
    struct DirectionalLightBuffer
    {
        XMFLOAT4 direction; // xyz=dir, w=intensity
        XMFLOAT4 color;
        XMFLOAT4 ambient;
    };
    struct DamageBufferType
    {
        XMFLOAT4 crackPositions[8]; // xyz=world pos, w=radius
        XMFLOAT4 crackParams[8];    // x=intensity, y=rotation, zw=pad
        int      crackCount;
        XMFLOAT3 _pad;
    };

    // ── GPU буферы (слоты совпадают с шейдером) ───────────────────────────────
    ID3D11Buffer* m_matrixBuffer = nullptr; // b0 VS
    ID3D11Buffer* m_lightPosBuffer = nullptr; // b1 VS+PS
    ID3D11Buffer* m_lightColorBuffer = nullptr; // b0 PS
    ID3D11Buffer* m_selectedBuffer = nullptr; // b3 PS
    ID3D11Buffer* m_directionalLightBuffer = nullptr; // b4 PS
    ID3D11Buffer* m_shadowBuffer = nullptr; // b5 PS
    ID3D11Buffer* m_pbrParamsBuffer = nullptr; // b6 PS
    ID3D11Buffer* m_damageBuffer = nullptr; // b7 PS ← трещины

    // ── Текстуры ──────────────────────────────────────────────────────────────
    ID3D11ShaderResourceView* m_defaultNormalSRV = nullptr;
    ID3D11ShaderResourceView* m_defaultMetalnessSRV = nullptr;
    ID3D11ShaderResourceView* m_defaultRoughnessSRV = nullptr;
    ID3D11ShaderResourceView* m_defaultCrackSRV = nullptr; // чёрный fallback (нет трещин)
    ID3D11ShaderResourceView* m_crackTextureSRV = nullptr; // внешняя, ApplicationClass владеет
    ID3D11ShaderResourceView* m_environmentMapSRV = nullptr; // внешняя skybox cubemap, ApplicationClass владеет

    ID3D11SamplerState* m_shadowSampler = nullptr;
};
