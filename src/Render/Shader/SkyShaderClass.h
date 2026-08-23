#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include "BaseShader.h"
using namespace DirectX;

class SkyShaderClass : public ShaderBaseClass
{
public:

    struct SkyConstantBuffer {
        XMMATRIX invViewProjection;
    };
    bool InitializeGeometry(ID3D11Device* device);
    void RenderGeometry(ID3D11DeviceContext* context);

    // Приватные члены:
    ID3D11Buffer* m_vertexBuffer = nullptr;
    ID3D11Buffer* m_indexBuffer = nullptr;
    UINT m_indexCount = 0;
    struct Vertex { XMFLOAT3 pos; };
    bool Initialize(ID3D11Device*, HWND);
    void Shutdown();
    bool Render(ID3D11DeviceContext* ctx, const XMMATRIX& invViewProjection);
    void SetSkyTexture(ID3D11ShaderResourceView* srv) { m_skySRV = srv; }

    ID3D11ShaderResourceView* GetSkyTexture() const { return m_skySRV; }
private:
    bool InitializeShader(ID3D11Device* device, HWND, const WCHAR*, const WCHAR*);
    void ShutdownShader();
    bool SetShaderParameters(ID3D11DeviceContext* ctx,
        const XMMATRIX& camRot, const XMMATRIX& invProj,
        const XMFLOAT3& sunDir, float sunIntensity);
    void RenderShader(ID3D11DeviceContext*, int);

    ID3D11ShaderResourceView* m_skySRV = nullptr;
    ID3D11SamplerState* m_skySampler = nullptr;

    
    ID3D11VertexShader* m_vertexShader = nullptr;
    ID3D11PixelShader* m_pixelShader = nullptr;
    ID3D11InputLayout* m_layout = nullptr;
    ID3D11Buffer* m_matrixBuffer = nullptr;
};
