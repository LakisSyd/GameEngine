// ShadowMap.cpp
#include "ShadowMap.h"

ShadowMap::ShadowMap()
    : m_depthStencilBuffer(nullptr)
    , m_depthStencilView(nullptr)
    , m_shaderResourceView(nullptr)
{
}

ShadowMap::~ShadowMap()
{
    Shutdown();
}

bool ShadowMap::Initialize(ID3D11Device* device, int width, int height)
{
    m_shadowMapWidth = width;
    m_shadowMapHeight = height;

    // 1. Создаём depth texture
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R24G8_TYPELESS; // 24 бита depth + 8 stencil
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

    if (FAILED(device->CreateTexture2D(&texDesc, nullptr, &m_depthStencilBuffer)))
        return false;

    // 2. Depth Stencil View (для записи глубины)
    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;

    if (FAILED(device->CreateDepthStencilView(m_depthStencilBuffer, &dsvDesc, &m_depthStencilView)))
        return false;

    // 3. Shader Resource View (для чтения в PS)
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    if (FAILED(device->CreateShaderResourceView(m_depthStencilBuffer, &srvDesc, &m_shaderResourceView)))
        return false;

    // 4. Viewport
    m_viewport.Width = (float)width;
    m_viewport.Height = (float)height;
    m_viewport.MinDepth = 0.0f;
    m_viewport.MaxDepth = 1.0f;
    m_viewport.TopLeftX = 0;
    m_viewport.TopLeftY = 0;

    return true;
}

void ShadowMap::Shutdown()
{
    if (m_shaderResourceView) { m_shaderResourceView->Release(); m_shaderResourceView = nullptr; }
    if (m_depthStencilView) { m_depthStencilView->Release(); m_depthStencilView = nullptr; }
    if (m_depthStencilBuffer) { m_depthStencilBuffer->Release(); m_depthStencilBuffer = nullptr; }
}

void ShadowMap::SetRenderTarget(ID3D11DeviceContext* context)
{
    context->OMSetRenderTargets(0, nullptr, m_depthStencilView);
    context->RSSetViewports(1, &m_viewport);
}

void ShadowMap::ClearRenderTarget(ID3D11DeviceContext* context)
{
    context->ClearDepthStencilView(m_depthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
}

void ShadowMap::SetLightViewProjection(const XMMATRIX& view, const XMMATRIX& proj)
{
    m_lightView = view;
    m_lightProjection = proj;
}
