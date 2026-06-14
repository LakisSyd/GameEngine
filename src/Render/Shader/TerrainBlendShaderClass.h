#pragma once
#include "BaseShader.h"

class TerrainBlendShaderClass : public ShaderBaseClass
{
public:
    TerrainBlendShaderClass() = default;
    ~TerrainBlendShaderClass() { Shutdown(); }

    bool Initialize(ID3D11Device* device, HWND hwnd) override;
    void Shutdown() override;
    ShaderType GetType() const override { return ShaderType::TerrainBlend; }

    bool Render(
        ID3D11DeviceContext* context,
        int indexCount,
        XMMATRIX world,
        XMMATRIX view,
        XMMATRIX projection,
        ID3D11ShaderResourceView* grassTexture,
        ID3D11ShaderResourceView* dirtTexture,
        ID3D11ShaderResourceView* maskTexture,
        ID3D11ShaderResourceView* grassNormal,
        ID3D11ShaderResourceView* dirtNormal,
        ID3D11ShaderResourceView* grassVariationTexture,
        ID3D11ShaderResourceView* grassVariationNormal,
        float tileScale,
        float normalStrength,
        float variationScale,
        float variationStrength,
        float brightness,
        float ambientBoost,
        XMFLOAT2 terrainMinXZ,
        XMFLOAT2 terrainSizeXZ);

    bool SetShadowMap(
        ID3D11DeviceContext* context,
        ID3D11ShaderResourceView* shadowMapSRV,
        const XMMATRIX& lightViewProj);

    bool SetDirectionalLight(
        ID3D11DeviceContext* context,
        const XMFLOAT3& direction,
        const XMFLOAT4& color,
        const XMFLOAT4& ambient,
        float intensity);

private:
    struct MatrixBuffer
    {
        XMMATRIX world;
        XMMATRIX view;
        XMMATRIX projection;
    };

    struct TerrainBuffer
    {
        XMFLOAT2 terrainMinXZ;
        XMFLOAT2 terrainSizeXZ;
        float tileScale;
        float normalStrength;
        float variationScale;
        float variationStrength;
        float brightness;
        float ambientBoost;
        XMFLOAT2 padding;
    };

    struct ShadowBuffer
    {
        XMMATRIX lightViewProj;
    };

    struct DirectionalLightBuffer
    {
        XMFLOAT4 direction;
        XMFLOAT4 color;
        XMFLOAT4 ambient;
    };

    ID3D11Buffer* m_matrixBuffer = nullptr;
    ID3D11Buffer* m_terrainBuffer = nullptr;
    ID3D11Buffer* m_shadowBuffer = nullptr;
    ID3D11Buffer* m_directionalLightBuffer = nullptr;
    ID3D11ShaderResourceView* m_defaultGrassSRV = nullptr;
    ID3D11ShaderResourceView* m_defaultDirtSRV = nullptr;
    ID3D11ShaderResourceView* m_defaultMaskSRV = nullptr;
    ID3D11ShaderResourceView* m_defaultNormalSRV = nullptr;
    ID3D11ShaderResourceView* m_shadowMapSRV = nullptr;
    ID3D11SamplerState* m_shadowSampler = nullptr;
};
