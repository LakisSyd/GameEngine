#include "FogShaderClass.h"
#include "helper.h"

FogShaderClass::FogShaderClass()
    : 
     m_matrixBuffer(nullptr)
    , m_fogBuffer(nullptr)
{
}

FogShaderClass::~FogShaderClass()
{
    Shutdown();
}

bool FogShaderClass::Initialize(ID3D11Device* device, HWND hwnd)
{
    wchar_t vsFilename[128];
    wchar_t psFilename[128];
    int error;

    error = wcscpy_s(vsFilename, 128, L"src/Render/Shader/Vs/fog.vs");
    if (error != 0) { MessageBox(hwnd, L"fog.vs not found", L"ERROR", MB_OK); return false; }

    error = wcscpy_s(psFilename, 128, L"src/Render/Shader/Ps/fog.ps");
    if (error != 0) { MessageBox(hwnd, L"fog.ps not found", L"ERROR", MB_OK); return false; }

    OutputDebugStringA("FogShaderClass: calling InitializeShader...\n");
    bool result = InitializeShader(device, hwnd, vsFilename, psFilename);
    if (!result)
    {
        OutputDebugStringA("FogShaderClass: InitializeShader FAILED!\n");
        return false;
    }
    OutputDebugStringA("FogShaderClass: InitializeShader OK\n");
    return true;
}

void FogShaderClass::Shutdown()
{
    ShutdownShader();
    ShaderBaseClass::Shutdown();
}

bool FogShaderClass::Render(ID3D11DeviceContext* deviceContext, int indexCount,
    XMMATRIX worldMatrix, XMMATRIX viewMatrix, XMMATRIX projectionMatrix,
    ID3D11ShaderResourceView* texture, float fogStart, float fogEnd)
{
    if (!SetShaderParameters(deviceContext, worldMatrix, viewMatrix, projectionMatrix,
        texture, fogStart, fogEnd))
    {
        return false;
    }
    RenderShader(deviceContext, indexCount);
    return true;
}

bool FogShaderClass::SetShaderParameters(ID3D11DeviceContext* deviceContext,
    XMMATRIX worldMatrix, XMMATRIX viewMatrix, XMMATRIX projectionMatrix,
    ID3D11ShaderResourceView* texture, float fogStart, float fogEnd)
{
    HRESULT result;
    D3D11_MAPPED_SUBRESOURCE mappedResource;

    // Транспонируем матрицы для HLSL
    worldMatrix      = XMMatrixTranspose(worldMatrix);
    viewMatrix       = XMMatrixTranspose(viewMatrix);
    projectionMatrix = XMMatrixTranspose(projectionMatrix);

    // ── 1. Буфер матриц → вершинный шейдер, слот 0 ──────────────────────────
    result = deviceContext->Map(m_matrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(result)) { return false; }

    MatrixBufferType* dataPtr = (MatrixBufferType*)mappedResource.pData;
    dataPtr->world      = worldMatrix;
    dataPtr->view       = viewMatrix;
    dataPtr->projection = projectionMatrix;

    deviceContext->Unmap(m_matrixBuffer, 0);
    deviceContext->VSSetConstantBuffers(0, 1, &m_matrixBuffer);

    // ── 2. Буфер тумана → вершинный шейдер, слот 1 ──────────────────────────
    result = deviceContext->Map(m_fogBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(result)) { return false; }

    FogBufferType* fogPtr = (FogBufferType*)mappedResource.pData;
    fogPtr->fogStart  = fogStart;
    fogPtr->fogEnd    = fogEnd;
    fogPtr->padding1  = 0.0f;
    fogPtr->padding2  = 0.0f;

    deviceContext->Unmap(m_fogBuffer, 0);
    deviceContext->VSSetConstantBuffers(1, 1, &m_fogBuffer);

    // ── 3. Текстура → пиксельный шейдер ─────────────────────────────────────
    deviceContext->PSSetShaderResources(0, 1, &texture);

    return true;
}

bool FogShaderClass::InitializeShader(ID3D11Device* device, HWND hwnd,
    WCHAR* vsFilename, WCHAR* psFilename)
{
    ID3D10Blob* errorMessage       = nullptr;
    ID3D10Blob* vertexShaderBuffer = nullptr;
    ID3D10Blob* pixelShaderBuffer  = nullptr;
    HRESULT result;

    // ── Компиляция вершинного шейдера ────────────────────────────────────────
    OutputDebugStringA("FogShaderClass: compiling VS...\n");
    result = D3DCompileFromFile(vsFilename, NULL, NULL, "FogVertexShader", "vs_5_0",
        D3D10_SHADER_ENABLE_STRICTNESS, 0, &vertexShaderBuffer, &errorMessage);

    if (FAILED(result))
    {
        if (errorMessage)
        {
            char* errors = (char*)errorMessage->GetBufferPointer();
            OutputDebugStringA("=== FOG VS COMPILE ERROR ===\n");
            OutputDebugStringA(errors);
            OutputDebugStringA("\n============================\n");
            errorMessage->Release();
        }
        else
        {
            OutputDebugStringA("FogShaderClass: fog.vs FILE NOT FOUND!\n");
            MessageBoxW(hwnd, vsFilename, L"fog.vs — файл не найден!", MB_OK);
        }
        return false;
    }
    OutputDebugStringA("FogShaderClass: VS compiled OK\n");

    // ── Компиляция пиксельного шейдера ───────────────────────────────────────
    OutputDebugStringA("FogShaderClass: compiling PS...\n");
    result = D3DCompileFromFile(psFilename, NULL, NULL, "FogPixelShader", "ps_5_0",
        D3D10_SHADER_ENABLE_STRICTNESS, 0, &pixelShaderBuffer, &errorMessage);

    if (FAILED(result))
    {
        if (errorMessage)
        {
            char* errors = (char*)errorMessage->GetBufferPointer();
            OutputDebugStringA("=== FOG PS COMPILE ERROR ===\n");
            OutputDebugStringA(errors);
            OutputDebugStringA("\n============================\n");
            errorMessage->Release();
        }
        else
        {
            OutputDebugStringA("FogShaderClass: fog.ps FILE NOT FOUND!\n");
            MessageBoxW(hwnd, psFilename, L"fog.ps — файл не найден!", MB_OK);
        }
        vertexShaderBuffer->Release();
        return false;
    }
    OutputDebugStringA("FogShaderClass: PS compiled OK\n");

    // ── Создаём шейдеры ──────────────────────────────────────────────────────
    if (FAILED(device->CreateVertexShader(vertexShaderBuffer->GetBufferPointer(),
        vertexShaderBuffer->GetBufferSize(), NULL, &m_vertexShader)))
    {
        OutputDebugStringA("FogShaderClass: CreateVertexShader FAILED!\n");
        vertexShaderBuffer->Release();
        pixelShaderBuffer->Release();
        return false;
    }

    if (FAILED(device->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(),
        pixelShaderBuffer->GetBufferSize(), NULL, &m_pixelShader)))
    {
        OutputDebugStringA("FogShaderClass: CreatePixelShader FAILED!\n");
        vertexShaderBuffer->Release();
        pixelShaderBuffer->Release();
        return false;
    }

    // ── Input Layout ─────────────────────────────────────────────────────────
    D3D11_INPUT_ELEMENT_DESC polygonLayout[2];
    polygonLayout[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
                         D3D11_INPUT_PER_VERTEX_DATA, 0 };
    polygonLayout[1] = { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
                         D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 };

    if (FAILED(device->CreateInputLayout(polygonLayout, 2,
        vertexShaderBuffer->GetBufferPointer(),
        vertexShaderBuffer->GetBufferSize(), &m_layout)))
    {
        OutputDebugStringA("FogShaderClass: CreateInputLayout FAILED!\n");
        vertexShaderBuffer->Release();
        pixelShaderBuffer->Release();
        return false;
    }

    vertexShaderBuffer->Release();
    pixelShaderBuffer->Release();

    // ── Буфер матриц (слот 0) ────────────────────────────────────────────────
    D3D11_BUFFER_DESC matrixBufferDesc = {};
    matrixBufferDesc.Usage          = D3D11_USAGE_DYNAMIC;
    matrixBufferDesc.ByteWidth      = sizeof(MatrixBufferType);
    matrixBufferDesc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    matrixBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    if (FAILED(device->CreateBuffer(&matrixBufferDesc, nullptr, &m_matrixBuffer)))
    {
        OutputDebugStringA("FogShaderClass: CreateBuffer(matrix) FAILED!\n");
        return false;
    }

    // ── Буфер тумана (слот 1) ────────────────────────────────────────────────
    D3D11_BUFFER_DESC fogBufferDesc = {};
    fogBufferDesc.Usage          = D3D11_USAGE_DYNAMIC;
    fogBufferDesc.ByteWidth      = sizeof(FogBufferType);
    fogBufferDesc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    fogBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    if (FAILED(device->CreateBuffer(&fogBufferDesc, nullptr, &m_fogBuffer)))
    {
        OutputDebugStringA("FogShaderClass: CreateBuffer(fog) FAILED!\n");
        return false;
    }

    // ── Сэмплер ──────────────────────────────────────────────────────────────
    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU       = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressV       = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressW       = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    samplerDesc.MaxLOD         = D3D11_FLOAT32_MAX;

    if (FAILED(device->CreateSamplerState(&samplerDesc, &m_sampleState)))
    {
        OutputDebugStringA("FogShaderClass: CreateSamplerState FAILED!\n");
        return false;
    }

    OutputDebugStringA("FogShaderClass: InitializeShader complete OK\n");
    return true;
}

void FogShaderClass::ShutdownShader()
{
    if (m_fogBuffer)     { m_fogBuffer->Release();     m_fogBuffer     = nullptr; }
    if (m_matrixBuffer)  { m_matrixBuffer->Release();  m_matrixBuffer  = nullptr; }
}

void FogShaderClass::RenderShader(ID3D11DeviceContext* deviceContext, int indexCount)
{
    deviceContext->IASetInputLayout(m_layout);
    deviceContext->VSSetShader(m_vertexShader, NULL, 0);
    deviceContext->PSSetShader(m_pixelShader, NULL, 0);
    deviceContext->PSSetSamplers(0, 1, &m_sampleState);
    deviceContext->DrawIndexed(indexCount, 0, 0);
}
