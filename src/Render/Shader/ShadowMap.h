// ShadowMap.h
#pragma once
#include <d3d11.h>
#include <DirectXMath.h>

using namespace DirectX;

class ShadowMap
{
public:
    ShadowMap();
    ~ShadowMap();

    bool Initialize(ID3D11Device* device, int width, int height);
    void Shutdown();
    XMMATRIX GetLightViewProjection() const
    {
        return XMMatrixMultiply(m_lightView, m_lightProjection);
    }

    ID3D11ShaderResourceView* GetShaderResourceView() const
    {
        return m_shaderResourceView;
    }
    // Установить как render target для shadow pass
    void SetRenderTarget(ID3D11DeviceContext* context);
    void ClearRenderTarget(ID3D11DeviceContext* context);

    // Получить SRV для чтения в main pass
   

    // Матрицы света (для трансформации в shadow space)
    void SetLightViewProjection(const XMMATRIX& view, const XMMATRIX& proj);
    

private:
    int m_shadowMapWidth;
    int m_shadowMapHeight;

    ID3D11Texture2D* m_depthStencilBuffer;
    ID3D11DepthStencilView* m_depthStencilView;
    ID3D11ShaderResourceView* m_shaderResourceView;
    D3D11_VIEWPORT m_viewport;

    XMMATRIX m_lightView;
    XMMATRIX m_lightProjection;
};
