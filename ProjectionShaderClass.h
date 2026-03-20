#pragma once
#include "BaseShader.h"

class projectionShaderClass : public ShaderBaseClass
{
    struct MatrixBufferType
    {
        XMMATRIX world;
        XMMATRIX view;
        XMMATRIX projection;
        XMMATRIX view2;        // ← Матрица вида проектора
        XMMATRIX projection2;  // ← Матрица проекции проектора
    };

    struct LightBufferType
    {
        XMFLOAT4 ambientColor;
    
          
        XMFLOAT4 diffuseColor;
        XMFLOAT3 lightDirection;
        float padding;
    };
    
 
    ID3D11SamplerState* m_sampleState = nullptr;
    ID3D11SamplerState* m_sampleState2 = nullptr;

public:
    projectionShaderClass() = default;
    ~projectionShaderClass() = default;

    bool Initialize(ID3D11Device* device, HWND hwnd) override;
    void Shutdown() override;

    bool Render(
        ID3D11DeviceContext* context, int indexCount,
        XMMATRIX world, XMMATRIX view, XMMATRIX projection,
        XMMATRIX view2, XMMATRIX projection2,
        ID3D11ShaderResourceView* texture,
        ID3D11ShaderResourceView* projectionTexture,
        XMFLOAT4 ambientColor, XMFLOAT4 diffuseColor,
        XMFLOAT3 lightDirection
    );

    ShaderType GetType() const override { return ShaderType::Projection; }

private:
    bool SetShaderParameters(
        ID3D11DeviceContext* context,
        XMMATRIX world, XMMATRIX view, XMMATRIX projection,
        XMMATRIX view2, XMMATRIX projection2,
        ID3D11ShaderResourceView* texture,
        ID3D11ShaderResourceView* projectionTexture,
        XMFLOAT4 ambientColor, XMFLOAT4 diffuseColor,
        XMFLOAT3 lightDirection
    );
    void RenderShader(ID3D11DeviceContext* context, int indexCount);

    bool InitializeShader(ID3D11Device* device, HWND hwnd,
        WCHAR* vsFilename, WCHAR* psFilename);

    void ShutdownShader();

private:
    ID3D11Buffer* m_matrixBuffer = nullptr;
    ID3D11Buffer* m_lightBuffer = nullptr;
};