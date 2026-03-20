////////////////////////////////////////////////////////////////////////////////
// WaterShaderClass.cpp Ч по туториалу 31 (rastertek.com)
////////////////////////////////////////////////////////////////////////////////
#include "WaterShaderClass.h"

static const wchar_t* WATER_VS = L"src/Render/Shader/Vs/Water.vs";
static const wchar_t* WATER_PS = L"src/Render/Shader/Ps/Water.ps";

WaterShaderClass::WaterShaderClass() {}

bool WaterShaderClass::Initialize(ID3D11Device* device, HWND hwnd)
{
    return InitializeShader(device, hwnd, WATER_VS, WATER_PS);
}

void WaterShaderClass::Shutdown()
{
    auto SafeRelease = [](auto*& ptr) { if (ptr) { ptr->Release(); ptr = nullptr; } };
    SafeRelease(m_waterBuffer);
    SafeRelease(m_reflectionBuffer);
    SafeRelease(m_matrixBuffer);
    ShaderBaseClass::Shutdown();
}

bool WaterShaderClass::Render(
    ID3D11DeviceContext* context,
    int                       indexCount,
    XMMATRIX                  worldMatrix,
    XMMATRIX                  viewMatrix,
    XMMATRIX                  projectionMatrix,
    XMMATRIX                  reflectionMatrix,
    ID3D11ShaderResourceView* reflectionTexture,
    ID3D11ShaderResourceView* refractionTexture,
    ID3D11ShaderResourceView* normalTexture,
    float                     waterTranslation,
    float                     reflectRefractScale)
{
    if (!SetShaderParameters(context,
        worldMatrix, viewMatrix, projectionMatrix, reflectionMatrix,
        reflectionTexture, refractionTexture, normalTexture,
        waterTranslation, reflectRefractScale))
        return false;

    RenderShader(context, indexCount);
    return true;
}

bool WaterShaderClass::InitializeShader(ID3D11Device* device, HWND hwnd,
    const wchar_t* vsFile, const wchar_t* psFile)
{
    ID3D10Blob* vsBlob = nullptr;
    ID3D10Blob* psBlob = nullptr;

    if (!CompileShader(vsFile, "WaterVertexShader", "vs_5_0", &vsBlob))
    {
        MessageBox(hwnd, vsFile, L"Water.vs Ч ошибка компил€ции", MB_OK);
        return false;
    }
    if (!CompileShader(psFile, "WaterPixelShader", "ps_5_0", &psBlob))
    {
        MessageBox(hwnd, psFile, L"Water.ps Ч ошибка компил€ции", MB_OK);
        vsBlob->Release();
        return false;
    }

    HRESULT hr;
    hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), NULL, &m_vertexShader);
    if (FAILED(hr)) { vsBlob->Release(); psBlob->Release(); return false; }

    hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), NULL, &m_pixelShader);
    if (FAILED(hr)) { vsBlob->Release(); psBlob->Release(); return false; }

    D3D11_INPUT_ELEMENT_DESC layout[2] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,                            D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    if (!CreateInputLayout(device, layout, 2, vsBlob)) { vsBlob->Release(); psBlob->Release(); return false; }

    vsBlob->Release();
    psBlob->Release();

    if (!CreateDefaultSampler(device))           return false;
    if (!CreateConstantBuffer(device, sizeof(MatrixBufferType), &m_matrixBuffer))     return false;
    if (!CreateConstantBuffer(device, sizeof(ReflectionBufferType), &m_reflectionBuffer)) return false;
    if (!CreateConstantBuffer(device, sizeof(WaterBufferType), &m_waterBuffer))      return false;

    return true;
}

bool WaterShaderClass::SetShaderParameters(
    ID3D11DeviceContext* context,
    XMMATRIX                  worldMatrix,
    XMMATRIX                  viewMatrix,
    XMMATRIX                  projectionMatrix,
    XMMATRIX                  reflectionMatrix,
    ID3D11ShaderResourceView* reflectionTexture,
    ID3D11ShaderResourceView* refractionTexture,
    ID3D11ShaderResourceView* normalTexture,
    float                     waterTranslation,
    float                     reflectRefractScale)
{
    // b0 VS Ч матрицы
    MatrixBufferType matrices;
    matrices.world = XMMatrixTranspose(worldMatrix);
    matrices.view = XMMatrixTranspose(viewMatrix);
    matrices.projection = XMMatrixTranspose(projectionMatrix);
    if (!UpdateConstantBuffer(context, m_matrixBuffer, matrices, 0, true)) return false;

    // b1 VS Ч матрица отражени€
    ReflectionBufferType reflData;
    reflData.reflection = XMMatrixTranspose(reflectionMatrix);
    if (!UpdateConstantBuffer(context, m_reflectionBuffer, reflData, 1, true)) return false;

    // t0, t1, t2 PS Ч текстуры
    context->PSSetShaderResources(0, 1, &reflectionTexture);
    context->PSSetShaderResources(1, 1, &refractionTexture);
    context->PSSetShaderResources(2, 1, &normalTexture);

    // b0 PS Ч параметры воды
    WaterBufferType waterData;
    waterData.waterTranslation = waterTranslation;
    waterData.reflectRefractScale = reflectRefractScale;
    waterData.padding = { 0.f, 0.f };
    if (!UpdateConstantBuffer(context, m_waterBuffer, waterData, 0, false)) return false;

    context->PSSetSamplers(0, 1, &m_sampleState);
    return true;
}

void WaterShaderClass::RenderShader(ID3D11DeviceContext* context, int indexCount)
{
    context->IASetInputLayout(m_layout);
    context->VSSetShader(m_vertexShader, NULL, 0);
    context->PSSetShader(m_pixelShader, NULL, 0);
    context->DrawIndexed(indexCount, 0, 0);
}