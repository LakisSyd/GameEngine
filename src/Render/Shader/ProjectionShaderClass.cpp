#include "ProjectionShaderClass.h"
#include "helper.h"



bool projectionShaderClass::Initialize(ID3D11Device* device, HWND hwnd)
{
    wchar_t vsFilename[128];
    wchar_t psFilename[128];

    if (wcscpy_s(vsFilename, 128, L"src/Render/Shader/Vs/projection.vs") != 0)
    {
        MessageBox(hwnd, L"projection.vs path error", L"ERROR", MB_OK);
        return false;
    }
    if (wcscpy_s(psFilename, 128, L"src/Render/Shader/Ps/projection.ps") != 0)
    {
        MessageBox(hwnd, L"projection.ps path error", L"ERROR", MB_OK);
        return false;
    }

    OutputDebugStringA("ProjectionShaderClass: calling InitializeShader...\n");
    bool result = InitializeShader(device, hwnd, vsFilename, psFilename);
    if (!result)
    {
        OutputDebugStringA("ProjectionShaderClass: InitializeShader FAILED!\n");
        return false;
    }
    OutputDebugStringA("ProjectionShaderClass: InitializeShader OK\n");
    return true;
}

void projectionShaderClass::Shutdown()
{
    ShutdownShader();
    ShaderBaseClass::Shutdown();
}

bool projectionShaderClass::Render(ID3D11DeviceContext* deviceContext, int indexCount,
    XMMATRIX worldMatrix, XMMATRIX viewMatrix, XMMATRIX projectionMatrix,
    XMMATRIX view2, XMMATRIX projection2,
    ID3D11ShaderResourceView* texture,
    ID3D11ShaderResourceView* projectionTexture,
    XMFLOAT4 ambientColor, XMFLOAT4 diffuseColor,
    XMFLOAT3 lightDirection)
{
    if (!SetShaderParameters(deviceContext,
        worldMatrix, viewMatrix, projectionMatrix,
        view2, projection2,
        texture, projectionTexture,
        ambientColor, diffuseColor, lightDirection))
    {
        return false;
    }
    RenderShader(deviceContext, indexCount);
    return true;
}

bool projectionShaderClass::SetShaderParameters(ID3D11DeviceContext* deviceContext,
    XMMATRIX worldMatrix, XMMATRIX viewMatrix, XMMATRIX projectionMatrix,
    XMMATRIX view2, XMMATRIX projection2,
    ID3D11ShaderResourceView* texture,
    ID3D11ShaderResourceView* projectionTexture,
    XMFLOAT4 ambientColor, XMFLOAT4 diffuseColor,
    XMFLOAT3 lightDirection)
{
    HRESULT result;
    D3D11_MAPPED_SUBRESOURCE mappedResource;

    // Транспонируем для HLSL (column-major)
    worldMatrix = XMMatrixTranspose(worldMatrix);
    viewMatrix = XMMatrixTranspose(viewMatrix);
    projectionMatrix = XMMatrixTranspose(projectionMatrix);
    view2 = XMMatrixTranspose(view2);
    projection2 = XMMatrixTranspose(projection2);

    // ── 1. Буфер матриц → VS слот 0 ─────────────────────────────────────────
    result = deviceContext->Map(m_matrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(result)) return false;

    MatrixBufferType* dataPtr = static_cast<MatrixBufferType*>(mappedResource.pData);
    dataPtr->world = worldMatrix;
    dataPtr->view = viewMatrix;
    dataPtr->projection = projectionMatrix;
    dataPtr->view2 = view2;
    dataPtr->projection2 = projection2;

    deviceContext->Unmap(m_matrixBuffer, 0);
    deviceContext->VSSetConstantBuffers(0, 1, &m_matrixBuffer);

    // ── 2. Буфер освещения → PS слот 0 ──────────────────────────────────────
    result = deviceContext->Map(m_lightBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(result)) return false;

    LightBufferType* lightPtr = static_cast<LightBufferType*>(mappedResource.pData);
    lightPtr->ambientColor = ambientColor;
    lightPtr->diffuseColor = diffuseColor;
    lightPtr->lightDirection = lightDirection;
    lightPtr->padding = 0.0f;

    deviceContext->Unmap(m_lightBuffer, 0);
    deviceContext->PSSetConstantBuffers(0, 1, &m_lightBuffer);

    // ── 3. Текстуры → PS ─────────────────────────────────────────────────────
    deviceContext->PSSetShaderResources(0, 1, &texture);
    deviceContext->PSSetShaderResources(1, 1, &projectionTexture);

    return true;
}

bool projectionShaderClass::InitializeShader(ID3D11Device* device, HWND hwnd,
    WCHAR* vsFilename, WCHAR* psFilename)
{
    ID3D10Blob* errorMessage = nullptr;
    ID3D10Blob* vertexShaderBuffer = nullptr;
    ID3D10Blob* pixelShaderBuffer = nullptr;

    // ── Компиляция VS ────────────────────────────────────────────────────────
    OutputDebugStringA("ProjectionShaderClass: compiling VS...\n");
    HRESULT result = D3DCompileFromFile(vsFilename, NULL, NULL,
        "ProjectionVertexShader", "vs_5_0",
        D3D10_SHADER_ENABLE_STRICTNESS, 0,
        &vertexShaderBuffer, &errorMessage);

    if (FAILED(result))
    {
        if (errorMessage)
        {
            OutputDebugStringA("=== PROJECTION VS COMPILE ERROR ===\n");
            OutputDebugStringA(static_cast<char*>(errorMessage->GetBufferPointer()));
            OutputDebugStringA("\n===================================\n");
            errorMessage->Release();
        }
        else
        {
            MessageBoxW(hwnd, vsFilename, L"projection.vs — файл не найден!", MB_OK);
        }
        return false;
    }
    OutputDebugStringA("ProjectionShaderClass: VS compiled OK\n");

    // ── Компиляция PS ────────────────────────────────────────────────────────
    OutputDebugStringA("ProjectionShaderClass: compiling PS...\n");
    result = D3DCompileFromFile(psFilename, NULL, NULL,
        "ProjectionPixelShader", "ps_5_0",
        D3D10_SHADER_ENABLE_STRICTNESS, 0,
        &pixelShaderBuffer, &errorMessage);

    if (FAILED(result))
    {
        if (errorMessage)
        {
            OutputDebugStringA("=== PROJECTION PS COMPILE ERROR ===\n");
            OutputDebugStringA(static_cast<char*>(errorMessage->GetBufferPointer()));
            OutputDebugStringA("\n===================================\n");
            errorMessage->Release();
        }
        else
        {
            MessageBoxW(hwnd, psFilename, L"projection.ps — файл не найден!", MB_OK);
        }
        vertexShaderBuffer->Release();
        return false;
    }
    OutputDebugStringA("ProjectionShaderClass: PS compiled OK\n");

    // ── Создаём шейдеры ──────────────────────────────────────────────────────
    if (FAILED(device->CreateVertexShader(vertexShaderBuffer->GetBufferPointer(),
        vertexShaderBuffer->GetBufferSize(), NULL, &m_vertexShader)))
    {
        vertexShaderBuffer->Release();
        pixelShaderBuffer->Release();
        return false;
    }

    if (FAILED(device->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(),
        pixelShaderBuffer->GetBufferSize(), NULL, &m_pixelShader)))
    {
        vertexShaderBuffer->Release();
        pixelShaderBuffer->Release();
        return false;
    }

    // ── Input Layout: POSITION + TEXCOORD + NORMAL ───────────────────────────
    D3D11_INPUT_ELEMENT_DESC polygonLayout[3] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
          D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0,
          D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
          D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    if (FAILED(device->CreateInputLayout(polygonLayout, 3,
        vertexShaderBuffer->GetBufferPointer(),
        vertexShaderBuffer->GetBufferSize(), &m_layout)))
    {
        vertexShaderBuffer->Release();
        pixelShaderBuffer->Release();
        return false;
    }

    vertexShaderBuffer->Release();
    pixelShaderBuffer->Release();

    // ── Буфер матриц (5 матриц × 64 байта = 320 байт, кратно 16) ────────────
    D3D11_BUFFER_DESC matrixBufferDesc = {};
    matrixBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    matrixBufferDesc.ByteWidth = sizeof(MatrixBufferType);
    matrixBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    matrixBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    if (FAILED(device->CreateBuffer(&matrixBufferDesc, nullptr, &m_matrixBuffer)))
    {
        OutputDebugStringA("ProjectionShaderClass: CreateBuffer(matrix) FAILED!\n");
        return false;
    }

    // ── Буфер освещения (48 байт, кратно 16) ─────────────────────────────────
    D3D11_BUFFER_DESC lightBufferDesc = {};
    lightBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    lightBufferDesc.ByteWidth = sizeof(LightBufferType);
    lightBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    lightBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    if (FAILED(device->CreateBuffer(&lightBufferDesc, nullptr, &m_lightBuffer)))
    {
        OutputDebugStringA("ProjectionShaderClass: CreateBuffer(light) FAILED!\n");
        return false;
    }

    // ── Сэмплер для обычной текстуры (s0) ────────────────────────────────────
    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    if (FAILED(device->CreateSamplerState(&samplerDesc, &m_sampleState)))
    {
        OutputDebugStringA("ProjectionShaderClass: CreateSamplerState(s0) FAILED!\n");
        return false;
    }

    // ── Сэмплер для проекционной текстуры (s1) — CLAMP, не WRAP ─────────────
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    // m_sampleState2 будет не-comparison по умолчанию (в некоторых шейдерах нужен простой CLAMP)
    // при необходимости можно настроить ComparisonFunc отдельно.

    if (FAILED(device->CreateSamplerState(&samplerDesc, &m_sampleState2)))
    {
        OutputDebugStringA("ProjectionShaderClass: CreateSamplerState(s1) FAILED!\n");
        return false;
    }

    // ── Сэмплер для сравнения (для shadow/compare sampling) → слот s7 ───────
    D3D11_SAMPLER_DESC cmpDesc = {};
    cmpDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
    cmpDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    cmpDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    cmpDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    cmpDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL; // типичная настройка для shadow maps
    cmpDesc.MinLOD = 0.0f;
    cmpDesc.MaxLOD = D3D11_FLOAT32_MAX;

    if (FAILED(device->CreateSamplerState(&cmpDesc, &m_cmpSampler)))
    {
        OutputDebugStringA("ProjectionShaderClass: CreateSamplerState(cmp) FAILED!\n");
        return false;
    }

    OutputDebugStringA("ProjectionShaderClass: InitializeShader complete OK\n");
    return true;
}

void projectionShaderClass::ShutdownShader()
{
    if (m_cmpSampler) { m_cmpSampler->Release(); m_cmpSampler = nullptr; }
    if (m_sampleState2) { m_sampleState2->Release(); m_sampleState2 = nullptr; }
    if (m_lightBuffer) { m_lightBuffer->Release();  m_lightBuffer = nullptr; }
    if (m_matrixBuffer) { m_matrixBuffer->Release(); m_matrixBuffer = nullptr; }
}

void projectionShaderClass::RenderShader(ID3D11DeviceContext* deviceContext, int indexCount)
{
    deviceContext->IASetInputLayout(m_layout);
    deviceContext->VSSetShader(m_vertexShader, NULL, 0);
    deviceContext->PSSetShader(m_pixelShader, NULL, 0);
    deviceContext->PSSetSamplers(0, 1, &m_sampleState);
    deviceContext->PSSetSamplers(1, 1, &m_sampleState2);
    // Привязать comparison-sampler в слот 7 — требуется шейдером, ожидающим sampler с comparison-фильтрацией.
    if (m_cmpSampler) deviceContext->PSSetSamplers(7, 1, &m_cmpSampler);
    deviceContext->DrawIndexed(indexCount, 0, 0);
}