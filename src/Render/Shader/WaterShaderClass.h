#pragma once
////////////////////////////////////////////////////////////////////////////////
// WaterShaderClass.h Ч по туториалу 31 (rastertek.com)
//
// VS слоты:
//   b0 Ч MatrixBuffer      : world / view / projection
//   b1 Ч ReflectionBuffer  : reflectionMatrix
//
// PS слоты:
//   b0 Ч WaterBuffer       : waterTranslation, reflectRefractScale
//
// “екстуры PS:
//   t0 Ч reflectionTexture
//   t1 Ч refractionTexture
//   t2 Ч normalTexture
////////////////////////////////////////////////////////////////////////////////

#include "BaseShader.h"
#include <d3d11.h>
#include <DirectXMath.h>

using namespace DirectX;

class WaterShaderClass : public ShaderBaseClass
{
private:
    struct MatrixBufferType
    {
        XMMATRIX world;
        XMMATRIX view;
        XMMATRIX projection;
    };

    struct ReflectionBufferType
    {
        XMMATRIX reflection;
    };

    struct WaterBufferType
    {
        float    waterTranslation;
        float    reflectRefractScale;
        XMFLOAT2 padding;
    };

public:
    WaterShaderClass();
    WaterShaderClass(const WaterShaderClass&) = default;
    ~WaterShaderClass() = default;

    bool Initialize(ID3D11Device* device, HWND hwnd) override;
    void Shutdown() override;
    ShaderType GetType() const override { return ShaderType::Water; }

    bool Render(
        ID3D11DeviceContext* context,
        int                        indexCount,
        XMMATRIX                   worldMatrix,
        XMMATRIX                   viewMatrix,
        XMMATRIX                   projectionMatrix,
        XMMATRIX                   reflectionMatrix,
        ID3D11ShaderResourceView* reflectionTexture,
        ID3D11ShaderResourceView* refractionTexture,
        ID3D11ShaderResourceView* normalTexture,
        float                      waterTranslation,
        float                      reflectRefractScale
    );

private:
    bool InitializeShader(ID3D11Device* device, HWND hwnd,
        const wchar_t* vsFile, const wchar_t* psFile);

    bool SetShaderParameters(
        ID3D11DeviceContext* context,
        XMMATRIX                   worldMatrix,
        XMMATRIX                   viewMatrix,
        XMMATRIX                   projectionMatrix,
        XMMATRIX                   reflectionMatrix,
        ID3D11ShaderResourceView* reflectionTexture,
        ID3D11ShaderResourceView* refractionTexture,
        ID3D11ShaderResourceView* normalTexture,
        float                      waterTranslation,
        float                      reflectRefractScale
    );

    void RenderShader(ID3D11DeviceContext* context, int indexCount);

private:
    ID3D11Buffer* m_matrixBuffer = nullptr; // b0 VS
    ID3D11Buffer* m_reflectionBuffer = nullptr; // b1 VS
    ID3D11Buffer* m_waterBuffer = nullptr; // b0 PS
};