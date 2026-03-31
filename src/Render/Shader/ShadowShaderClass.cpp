#include "ShadowShaderClass.h"

ShadowShaderClass::ShadowShaderClass()
    : m_matrixBuffer(nullptr)
{
}

ShadowShaderClass::~ShadowShaderClass()
{
    Shutdown();
}

bool ShadowShaderClass::Initialize(ID3D11Device* device, HWND hwnd)
{
    return InitializeShader(device, hwnd,
        L"src/Render/Shader/Vs/shadow.vs",
        L"src/Render/Shader/Ps/shadow.ps");
}

void ShadowShaderClass::Shutdown()
{
    ShutdownShader();
    ShaderBaseClass::Shutdown();
}

bool ShadowShaderClass::InitializeShader(
    ID3D11Device* device, HWND hwnd,
    const WCHAR* vsFilename, const WCHAR* psFilename)
{
    ID3D10Blob* vsBlob = nullptr;
    ID3D10Blob* psBlob = nullptr;

    // Компилируем vertex shader
    if (!CompileShader(vsFilename, "ShadowVertexShader", "vs_5_0", &vsBlob))
        return false;

    // Input layout (только позиция нужна)
    D3D11_INPUT_ELEMENT_DESC polygonLayout[1];
    polygonLayout[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
                         D3D11_INPUT_PER_VERTEX_DATA, 0 };

    if (!CreateInputLayout(device, polygonLayout, 1, vsBlob))
    {
        vsBlob->Release();
        return false;
    }

    // Создаём vertex shader
    HRESULT result = device->CreateVertexShader(
        vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(),
        nullptr,
        &m_vertexShader);

    if (FAILED(result))
    {
        vsBlob->Release();
        return false;
    }

    // Компилируем pixel shader (может быть пустым)
    if (!CompileShader(psFilename, "ShadowPixelShader", "ps_5_0", &psBlob))
    {
        vsBlob->Release();
        return false;
    }

    result = device->CreatePixelShader(
        psBlob->GetBufferPointer(),
        psBlob->GetBufferSize(),
        nullptr,
        &m_pixelShader);

    vsBlob->Release();
    psBlob->Release();

    if (FAILED(result))
        return false;

    // Создаём constant buffer
    if (!CreateConstantBuffer(device, sizeof(MatrixBufferType), &m_matrixBuffer))
        return false;

    return true;
}

void ShadowShaderClass::ShutdownShader()
{
    if (m_matrixBuffer)
    {
        m_matrixBuffer->Release();
        m_matrixBuffer = nullptr;
    }
}

bool ShadowShaderClass::Render(
    ID3D11DeviceContext* context,
    int indexCount,
    const XMMATRIX& world,
    const XMMATRIX& lightViewProj)
{
    if (!SetShaderParameters(context, world, lightViewProj))
        return false;

    RenderShader(context, indexCount);
    return true;
}

bool ShadowShaderClass::SetShaderParameters(
    ID3D11DeviceContext* context,
    const XMMATRIX& world,
    const XMMATRIX& lightViewProj)
{
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT result = context->Map(m_matrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(result))
        return false;

    MatrixBufferType* data = (MatrixBufferType*)mapped.pData;
    data->world = XMMatrixTranspose(world);
    data->lightViewProj = XMMatrixTranspose(lightViewProj);

    context->Unmap(m_matrixBuffer, 0);
    context->VSSetConstantBuffers(0, 1, &m_matrixBuffer);

    return true;
}

void ShadowShaderClass::RenderShader(ID3D11DeviceContext* context, int indexCount)
{
    BindShader(context);
    context->DrawIndexed(indexCount, 0, 0);
}
