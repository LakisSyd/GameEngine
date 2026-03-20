#include "GlassShaderClass.h"

bool GlassShaderClass::Initialize(ID3D11Device* device, HWND hwnd)
{
    // Glass вершинный шейдер использует только POSITION и TEXCOORD,
    // но вершинный буфер AssimpModelClass содержит {POSITION, TEXCOORD, NORMAL}.
    // Объявляем 3 элемента чтобы input layout совпал со stride вершинного буфера.
    D3D11_INPUT_ELEMENT_DESC polygonLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,                            D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    ID3D10Blob* vsBlob = nullptr;
    if (!CompileShader(L"src/Render/Shader/Vs/glass.vs", "GlassVertexShader", "vs_5_0", &vsBlob))
        return false;

    ID3D10Blob* psBlob = nullptr;
    if (!CompileShader(L"src/Render/Shader/Ps/glass.ps", "GlassPixelShader", "ps_5_0", &psBlob))
    {
        vsBlob->Release();
        return false;
    }

    HRESULT hr;
    hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_vertexShader);
    if (FAILED(hr)) { vsBlob->Release(); psBlob->Release(); return false; }

    hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_pixelShader);
    if (FAILED(hr)) { vsBlob->Release(); psBlob->Release(); return false; }

    // ✅ 5 элементов — совпадает с вершинным буфером AssimpModelClass
    if (!CreateInputLayout(device, polygonLayout, 3, vsBlob))
    {
        vsBlob->Release(); psBlob->Release();
        return false;
    }

    vsBlob->Release();
    psBlob->Release();

    if (!CreateDefaultSampler(device))                                            return false;
    if (!CreateConstantBuffer(device, sizeof(MatrixBufferType), &m_matrixBuffer)) return false;
    if (!CreateConstantBuffer(device, sizeof(GlassBufferType), &m_glassBuffer))   return false;

    return true;
}

void GlassShaderClass::Shutdown()
{
    if (m_glassBuffer) { m_glassBuffer->Release();  m_glassBuffer = nullptr; }
    if (m_matrixBuffer) { m_matrixBuffer->Release(); m_matrixBuffer = nullptr; }
    ShaderBaseClass::Shutdown();
}

bool GlassShaderClass::Render(
    ID3D11DeviceContext* context, int indexCount,
    XMMATRIX world, XMMATRIX view, XMMATRIX projection,
    ID3D11ShaderResourceView* colorTex,
    ID3D11ShaderResourceView* normalTex,
    ID3D11ShaderResourceView* refractionTex,
    float refractionScale)
{
    if (!SetShaderParameters(context, world, view, projection,
        colorTex, normalTex, refractionTex, refractionScale))
        return false;

    RenderShader(context, indexCount);
    return true;
}

bool GlassShaderClass::SetShaderParameters(
    ID3D11DeviceContext* context,
    XMMATRIX world, XMMATRIX view, XMMATRIX projection,
    ID3D11ShaderResourceView* colorTex,
    ID3D11ShaderResourceView* normalTex,
    ID3D11ShaderResourceView* refractionTex,
    float refractionScale)
{
    MatrixBufferType mb;
    mb.world = XMMatrixTranspose(world);
    mb.view = XMMatrixTranspose(view);
    mb.projection = XMMatrixTranspose(projection);
    if (!UpdateConstantBuffer(context, m_matrixBuffer, mb, 0, true)) return false;

    GlassBufferType gb;
    gb.refractionScale = refractionScale;
    gb.padding = { 0, 0, 0 };
    if (!UpdateConstantBuffer(context, m_glassBuffer, gb, 0, false)) return false;

    context->PSSetShaderResources(0, 1, &colorTex);
    context->PSSetShaderResources(1, 1, &normalTex);
    context->PSSetShaderResources(2, 1, &refractionTex);

    return true;
}

void GlassShaderClass::RenderShader(ID3D11DeviceContext* context, int indexCount)
{
    context->IASetInputLayout(m_layout);
    context->VSSetShader(m_vertexShader, nullptr, 0);
    context->PSSetShader(m_pixelShader, nullptr, 0);
    context->PSSetSamplers(0, 1, &m_sampleState);
    context->DrawIndexed(indexCount, 0, 0);
}