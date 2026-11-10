#pragma once

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

#include <d3d11.h>
#include <directxmath.h>
using namespace DirectX;

class D3DClass {
public:
    D3DClass();
    D3DClass(const D3DClass&) = delete;
    D3DClass& operator=(const D3DClass&) = delete;
    ~D3DClass();

    bool Initialize(int screenWidth, int screenHeight, bool vsync, HWND hwnd,
        bool fullscreen, float screenDepth, float screenNear);

    void BeginScene(float red, float green, float blue, float alpha);
    void EndScene();
    void Shutdown();

    IDXGISwapChain* GetSwapChain() { return m_swapChain; }

    ID3D11Device* GetDevice();
    ID3D11DeviceContext* GetDeviceContext();

    void GetProjectionMatrix(XMMATRIX&);
    void GetWorldMatrix(XMMATRIX&);
    void GetOrthoMatrix(XMMATRIX&);  // ✅ Уже есть — для 2D

    void GetVideoCardInfo(char*, int&);
    void SetBackBufferRenderTarget();
    void ResetViewport();
    void ClearDepthBuffer();

    // Переключение Z-буфера для 2D/3D
    void TurnZBufferOn();
    void TurnZBufferOff();

    void TurnAlphaBlendingOn();

    void TurnAlphaBlendingOff();

private:
    bool m_vsync_enabled = false;
    int m_videoCardMemory = 0;
    char m_videoCardDescription[128] = {};

    IDXGISwapChain* m_swapChain = nullptr;
    ID3D11Device* m_device = nullptr;
    ID3D11DeviceContext* m_deviceContext = nullptr;
    ID3D11RenderTargetView* m_renderTargetView = nullptr;
    ID3D11Texture2D* m_depthStencilBuffer = nullptr;

    ID3D11BlendState* m_alphaBlendState = nullptr;

    ID3D11DepthStencilState* m_depthStencilState = nullptr;
    ID3D11DepthStencilState* m_depthDisabledStencilState = nullptr;

    ID3D11DepthStencilView* m_depthStencilView = nullptr;
    ID3D11RasterizerState* m_rasterState = nullptr;

    XMMATRIX m_projectionMatrix = XMMatrixIdentity();
    XMMATRIX m_worldMatrix = XMMatrixIdentity();
    XMMATRIX m_orthoMatrix = XMMatrixIdentity();

    D3D11_VIEWPORT m_viewport{};
};
