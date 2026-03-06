#include "RenderTextureClass.h"

RenderTextureClass::RenderTextureClass()
{
    m_renderTargetTexture = nullptr;
    m_renderTargetView = nullptr;
    m_shaderResourceView = nullptr;
    m_depthStencilBuffer = nullptr;
    m_depthStencilView = nullptr;
}

RenderTextureClass::RenderTextureClass(const RenderTextureClass&)
{
}

RenderTextureClass::~RenderTextureClass()
{
    Shutdown();
}

bool RenderTextureClass::Initialize(ID3D11Device* device, int textureWidth,
    int textureHeight, float screenDepth,
    float screenNear, int format)
{
    D3D11_TEXTURE2D_DESC textureDesc;
    HRESULT result;
    D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
    D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc;
    D3D11_TEXTURE2D_DESC depthBufferDesc;
    D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc;
    DXGI_FORMAT textureFormat;

    // Выбор формата текстуры
    switch (format)
    {
    case 1:
        textureFormat = DXGI_FORMAT_R8G8B8A8_UNORM;  // Стандартный RGBA
        break;
    default:
        textureFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        break;
    }

    // Сохраняем размеры
    m_textureWidth = textureWidth;
    m_textureHeight = textureHeight;

    // 🔹 1. Создаём саму текстуру для рендеринга
    ZeroMemory(&textureDesc, sizeof(textureDesc));
    textureDesc.Width = textureWidth;
    textureDesc.Height = textureHeight;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = textureFormat;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    // 👇 Ключевой момент: текстура должна поддерживать и рендеринг, и чтение в шейдере
    textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    textureDesc.CPUAccessFlags = 0;
    textureDesc.MiscFlags = 0;

    result = device->CreateTexture2D(&textureDesc, NULL, &m_renderTargetTexture);
    if (FAILED(result)) { return false; }

    // 🔹 2. Создаём представление для рендеринга (RenderTargetView)
    renderTargetViewDesc.Format = textureDesc.Format;
    renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    renderTargetViewDesc.Texture2D.MipSlice = 0;

    result = device->CreateRenderTargetView(m_renderTargetTexture,
        &renderTargetViewDesc,
        &m_renderTargetView);
    if (FAILED(result)) { return false; }

    // 🔹 3. Создаём представление для шейдеров (ShaderResourceView)
    shaderResourceViewDesc.Format = textureDesc.Format;
    shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    shaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
    shaderResourceViewDesc.Texture2D.MipLevels = 1;

    result = device->CreateShaderResourceView(m_renderTargetTexture,
        &shaderResourceViewDesc,
        &m_shaderResourceView);
    if (FAILED(result)) { return false; }

    // 🔹 4. Создаём буфер глубины для корректной отрисовки 3D-сцены
    ZeroMemory(&depthBufferDesc, sizeof(depthBufferDesc));
    depthBufferDesc.Width = textureWidth;
    depthBufferDesc.Height = textureHeight;
    depthBufferDesc.MipLevels = 1;
    depthBufferDesc.ArraySize = 1;
    depthBufferDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;  // 24-бит глубина + 8-бит шаблон
    depthBufferDesc.SampleDesc.Count = 1;
    depthBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    depthBufferDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    result = device->CreateTexture2D(&depthBufferDesc, NULL, &m_depthStencilBuffer);
    if (FAILED(result)) { return false; }

    // 🔹 5. Создаём представление буфера глубины
    ZeroMemory(&depthStencilViewDesc, sizeof(depthStencilViewDesc));
    depthStencilViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    depthStencilViewDesc.Texture2D.MipSlice = 0;

    result = device->CreateDepthStencilView(m_depthStencilBuffer,
        &depthStencilViewDesc,
        &m_depthStencilView);
    if (FAILED(result)) { return false; }

    // 🔹 6. Настраиваем вьюпорт под размеры текстуры
    m_viewport.Width = (float)textureWidth;
    m_viewport.Height = (float)textureHeight;
    m_viewport.MinDepth = 0.0f;
    m_viewport.MaxDepth = 1.0f;
    m_viewport.TopLeftX = 0;
    m_viewport.TopLeftY = 0;

    // 🔹 7. Создаём матрицы проекции с учётом аспекта текстуры
    float aspectRatio = (float)textureWidth / (float)textureHeight;
    m_projectionMatrix = XMMatrixPerspectiveFovLH(
        XM_PI / 4.0f, aspectRatio, screenNear, screenDepth);
    m_orthoMatrix = XMMatrixOrthographicLH(
        (float)textureWidth, (float)textureHeight, screenNear, screenDepth);

    return true;
}


void RenderTextureClass::SetRenderTarget(ID3D11DeviceContext* deviceContext)
{
    deviceContext->OMSetRenderTargets(1, &m_renderTargetView, m_depthStencilView);

    // 👇 Устанавливаем вьюпорт под размеры текстуры
    deviceContext->RSSetViewports(1, &m_viewport);
}

ID3D11ShaderResourceView* RenderTextureClass::GetShaderResourceView()
{
    // Возвращаем представление, которое можно передать в шейдер как обычную текстуру
    return m_shaderResourceView;
}

void RenderTextureClass::ClearRenderTarget(ID3D11DeviceContext* deviceContext,
    float red, float green, float blue, float alpha)
{
    float color[4] = { red, green, blue, alpha };
    deviceContext->ClearRenderTargetView(m_renderTargetView, color);
    deviceContext->ClearDepthStencilView(m_depthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
}

void RenderTextureClass::GetProjectionMatrix(XMMATRIX& projectionMatrix)
{
    projectionMatrix = m_projectionMatrix;
}

void RenderTextureClass::GetOrthoMatrix(XMMATRIX& orthoMatrix)
{
    orthoMatrix = m_orthoMatrix;
}

int RenderTextureClass::GetTextureWidth()
{
    return m_textureWidth;
}

int RenderTextureClass::GetTextureHeight()
{
    return m_textureHeight;
}

void RenderTextureClass::Shutdown()
{
    if (m_depthStencilView) { m_depthStencilView->Release();    m_depthStencilView = nullptr; }
    if (m_depthStencilBuffer) { m_depthStencilBuffer->Release();  m_depthStencilBuffer = nullptr; }
    if (m_shaderResourceView) { m_shaderResourceView->Release();  m_shaderResourceView = nullptr; }
    if (m_renderTargetView) { m_renderTargetView->Release();    m_renderTargetView = nullptr; }
    if (m_renderTargetTexture) { m_renderTargetTexture->Release(); m_renderTargetTexture = nullptr; }
}