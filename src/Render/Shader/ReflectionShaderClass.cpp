#include "ReflectionShaderClass.h"
#include "helper.h"

static const wchar_t* ReflVsPath = L"src/Render/Shader/Vs/reflection.vs";
static const wchar_t* ReflPsPath = L"src/Render/Shader/Ps/reflection.ps";

bool ReflectionShaderClass::Initialize(ID3D11Device* device, HWND hwnd)
{
    return InitializeShader(device, hwnd, ReflVsPath, ReflPsPath);
}

void ReflectionShaderClass::Shutdown()
{
    if (m_colorBuffer) { m_colorBuffer->Release();   m_colorBuffer = nullptr; }
    if (m_reflectBuffer) { m_reflectBuffer->Release(); m_reflectBuffer = nullptr; }
    if (m_matrixBuffer) { m_matrixBuffer->Release();  m_matrixBuffer = nullptr; }
    ShaderBaseClass::Shutdown();
}

bool ReflectionShaderClass::Render(ID3D11DeviceContext* context, int indexCount,
    XMMATRIX world, XMMATRIX view, XMMATRIX projection,
    XMMATRIX reflectionMatrix,
    ID3D11ShaderResourceView* texture,
    ID3D11ShaderResourceView* reflectionTexture,
    float reflectivity)
{
    DX_CHECK(SetShaderParameters(context, world, view, projection,
        reflectionMatrix, texture, reflectionTexture, reflectivity));
    RenderShader(context, indexCount);
    return true;
}

bool ReflectionShaderClass::InitializeShader(ID3D11Device* device, HWND hwnd,
    const wchar_t* vsFile, const wchar_t* psFile)
{
    ID3D10Blob* vsBlob = nullptr;
    ID3D10Blob* psBlob = nullptr;

    if (!CompileShader(vsFile, "ReflectionVertexShader", "vs_5_0", &vsBlob)) return false;
    if (!CompileShader(psFile, "ReflectionPixelShader", "ps_5_0", &psBlob)) return false;

    DX_CHECK(device->CreateVertexShader(vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(), nullptr, &m_vertexShader));
    DX_CHECK(device->CreatePixelShader(psBlob->GetBufferPointer(),
        psBlob->GetBufferSize(), nullptr, &m_pixelShader));

    D3D11_INPUT_ELEMENT_DESC layout[3] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
          D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT,
          D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT,
          D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    if (!CreateInputLayout(device, layout, 3, vsBlob)) return false;

    vsBlob->Release();
    psBlob->Release();

    if (!CreateConstantBuffer(device, sizeof(MatrixBufferType), &m_matrixBuffer))  return false;
    if (!CreateConstantBuffer(device, sizeof(ReflectionBufferType), &m_reflectBuffer)) return false;
    if (!CreateConstantBuffer(device, sizeof(ReflectColorBufferType), &m_colorBuffer))   return false;
    if (!CreateDefaultSampler(device)) return false;
    if (!CreateFogBuffer(device))      return false;

    return true;
}

bool ReflectionShaderClass::SetShaderParameters(ID3D11DeviceContext* context,
    XMMATRIX world, XMMATRIX view, XMMATRIX projection,
    XMMATRIX reflectionMatrix,
    ID3D11ShaderResourceView* texture,
    ID3D11ShaderResourceView* reflectionTexture,
    float reflectivity)
{
    // ── VS b0: матрицы ───────────────────────────────────────────────────────
    MatrixBufferType mb;
    mb.world = XMMatrixTranspose(world);
    mb.view = XMMatrixTranspose(view);
    mb.projection = XMMatrixTranspose(projection);
    if (!UpdateConstantBuffer(context, m_matrixBuffer, mb, 0, true)) return false;

    // ── VS b1: матрица отражения ─────────────────────────────────────────────
    ReflectionBufferType rb;
    rb.reflectionMatrix = XMMatrixTranspose(reflectionMatrix);
    if (!UpdateConstantBuffer(context, m_reflectBuffer, rb, 1, true)) return false;

    // ── PS b0: степень зеркальности ─────────────────────────────────────────
    ReflectColorBufferType cb;
    cb.reflectivity = reflectivity;
    cb.padding = XMFLOAT3(0, 0, 0);
    if (!UpdateConstantBuffer(context, m_colorBuffer, cb, 0, false)) return false;

    // ── Текстуры ─────────────────────────────────────────────────────────────
    context->PSSetShaderResources(0, 1, &texture);           // t0 — пол
    context->PSSetShaderResources(1, 1, &reflectionTexture); // t1 — отражение

    return true;
}

void ReflectionShaderClass::RenderShader(ID3D11DeviceContext* context, int indexCount)
{
    context->IASetInputLayout(m_layout);
    context->VSSetShader(m_vertexShader, nullptr, 0);
    context->PSSetShader(m_pixelShader, nullptr, 0);
    context->PSSetSamplers(0, 1, &m_sampleState);
    context->DrawIndexed(indexCount, 0, 0);
}