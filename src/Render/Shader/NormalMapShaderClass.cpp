#include "normalmapshaderclass.h"

NormalMapShaderClass::NormalMapShaderClass()
    : m_vertexShader(nullptr)
    , m_pixelShader(nullptr)
    , m_layout(nullptr)
    , m_matrixBuffer(nullptr)
    , m_lightBuffer(nullptr)
    , m_sampleState(nullptr)
{
}

NormalMapShaderClass::NormalMapShaderClass(const NormalMapShaderClass&)
{
}

NormalMapShaderClass::~NormalMapShaderClass()
{
}

bool NormalMapShaderClass::Initialize(ID3D11Device* device, HWND hwnd)
{
    // Пути к шейдерам (расположены в папке Vs и Ps)
    return InitializeShader(device, hwnd,
        L"src/Render/Shader/Vs/Normalmap.vs",
        L"src/Render/Shader/Ps/Normalmap.ps"
    );
}

void NormalMapShaderClass::ShutDown()
{
    ShutdownShader();
}

bool NormalMapShaderClass::Render(
    ID3D11DeviceContext* deviceContext,
    int indexCount,
    XMMATRIX worldMatrix,
    XMMATRIX viewMatrix,
    XMMATRIX projectionMatrix,
    ID3D11ShaderResourceView* texture,
    ID3D11ShaderResourceView* normalMap,
    XMFLOAT3 lightDirection,
    XMFLOAT4 diffuseColor)
{
    if (!SetShaderParameters(
        deviceContext,
        worldMatrix, viewMatrix, projectionMatrix,
        texture, normalMap,
        lightDirection, diffuseColor))
    {
        return false;
    }

    RenderShader(deviceContext, indexCount);
    return true;
}

bool NormalMapShaderClass::InitializeShader(ID3D11Device* device, HWND hwnd, const WCHAR* vsFilename, const WCHAR* psFilename)
{
    HRESULT result;
    ID3D10Blob* errorMessage = nullptr;
    ID3D10Blob* vertexShaderBuffer = nullptr;
    ID3D10Blob* pixelShaderBuffer = nullptr;

    // Компилируем вершинный шейдер
    result = D3DCompileFromFile(
        vsFilename, NULL, NULL, "NormalMapVertexShader", "vs_5_0",
        D3D10_SHADER_ENABLE_STRICTNESS, 0,
        &vertexShaderBuffer, &errorMessage);

    if (FAILED(result)) {
        if (errorMessage) {
            OutputShaderErrorMessage(errorMessage, hwnd, (WCHAR*)vsFilename);
            errorMessage->Release();
        }
        else {
            MessageBox(hwnd, vsFilename, L"Missing Shader File", MB_OK);
        }
        return false;
    }

    // Компилируем пиксельный шейдер
    result = D3DCompileFromFile(
        psFilename, NULL, NULL, "NormalMapPixelShader", "ps_5_0",
        D3D10_SHADER_ENABLE_STRICTNESS, 0,
        &pixelShaderBuffer, &errorMessage);

    if (FAILED(result)) {
        if (errorMessage) {
            OutputShaderErrorMessage(errorMessage, hwnd, (WCHAR*)psFilename);
            errorMessage->Release();
        }
        else {
            MessageBox(hwnd, psFilename, L"Missing Shader File", MB_OK);
        }
        return false;
    }

    // Создаём вершинный шейдер
    result = device->CreateVertexShader(
        vertexShaderBuffer->GetBufferPointer(),
        vertexShaderBuffer->GetBufferSize(),
        NULL, &m_vertexShader);

    if (FAILED(result)) {
        return false;
    }

    // Создаём пиксельный шейдер
    result = device->CreatePixelShader(
        pixelShaderBuffer->GetBufferPointer(),
        pixelShaderBuffer->GetBufferSize(),
        NULL, &m_pixelShader);

    if (FAILED(result)) {
        return false;
    }

    // Определяем описание входного формата вершин
    D3D11_INPUT_ELEMENT_DESC polygonLayout[5];
    
    // POSITION
    polygonLayout[0].SemanticName = "POSITION";
    polygonLayout[0].SemanticIndex = 0;
    polygonLayout[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    polygonLayout[0].InputSlot = 0;
    polygonLayout[0].AlignedByteOffset = 0;
    polygonLayout[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
    polygonLayout[0].InstanceDataStepRate = 0;

    // TEXCOORD
    polygonLayout[1].SemanticName = "TEXCOORD";
    polygonLayout[1].SemanticIndex = 0;
    polygonLayout[1].Format = DXGI_FORMAT_R32G32_FLOAT;
    polygonLayout[1].InputSlot = 0;
    polygonLayout[1].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
    polygonLayout[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
    polygonLayout[1].InstanceDataStepRate = 0;

    // NORMAL
    polygonLayout[2].SemanticName = "NORMAL";
    polygonLayout[2].SemanticIndex = 0;
    polygonLayout[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    polygonLayout[2].InputSlot = 0;
    polygonLayout[2].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
    polygonLayout[2].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
    polygonLayout[2].InstanceDataStepRate = 0;

    // TANGENT
    polygonLayout[3].SemanticName = "TANGENT";
    polygonLayout[3].SemanticIndex = 0;
    polygonLayout[3].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    polygonLayout[3].InputSlot = 0;
    polygonLayout[3].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
    polygonLayout[3].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
    polygonLayout[3].InstanceDataStepRate = 0;

    // BINORMAL
    polygonLayout[4].SemanticName = "BINORMAL";
    polygonLayout[4].SemanticIndex = 0;
    polygonLayout[4].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    polygonLayout[4].InputSlot = 0;
    polygonLayout[4].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
    polygonLayout[4].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
    polygonLayout[4].InstanceDataStepRate = 0;

    unsigned int numElements = 5;

    // Создаём input layout
    result = device->CreateInputLayout(
        polygonLayout, numElements,
        vertexShaderBuffer->GetBufferPointer(),
        vertexShaderBuffer->GetBufferSize(),
        &m_layout);

    if (FAILED(result)) {
        return false;
    }

    // Освобождаем буферы шейдеров
    vertexShaderBuffer->Release();
    vertexShaderBuffer = nullptr;

    pixelShaderBuffer->Release();
    pixelShaderBuffer = nullptr;

    // Описание буфера матриц
    D3D11_BUFFER_DESC matrixBufferDesc;
    matrixBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    matrixBufferDesc.ByteWidth = sizeof(MatrixBufferType);
    matrixBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    matrixBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    matrixBufferDesc.MiscFlags = 0;

    result = device->CreateBuffer(&matrixBufferDesc, NULL, &m_matrixBuffer);
    if (FAILED(result)) {
        return false;
    }

    // Описание буфера света
    D3D11_BUFFER_DESC lightBufferDesc;
    lightBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    lightBufferDesc.ByteWidth = sizeof(LightBufferType);
    lightBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    lightBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    lightBufferDesc.MiscFlags = 0;

    result = device->CreateBuffer(&lightBufferDesc, NULL, &m_lightBuffer);
    if (FAILED(result)) {
        return false;
    }

    // Описание сэмплера текстур
    D3D11_SAMPLER_DESC samplerDesc;
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.MipLODBias = 0.0f;
    samplerDesc.MaxAnisotropy = 1;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    samplerDesc.BorderColor[0] = 0;
    samplerDesc.BorderColor[1] = 0;
    samplerDesc.BorderColor[2] = 0;
    samplerDesc.BorderColor[3] = 0;
    samplerDesc.MinLOD = 0;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    result = device->CreateSamplerState(&samplerDesc, &m_sampleState);
    if (FAILED(result)) {
        return false;
    }

    return true;
}

void NormalMapShaderClass::ShutdownShader()
{
    if (m_sampleState) {
        m_sampleState->Release();
        m_sampleState = nullptr;
    }

    if (m_lightBuffer) {
        m_lightBuffer->Release();
        m_lightBuffer = nullptr;
    }

    if (m_matrixBuffer) {
        m_matrixBuffer->Release();
        m_matrixBuffer = nullptr;
    }

    if (m_layout) {
        m_layout->Release();
        m_layout = nullptr;
    }

    if (m_pixelShader) {
        m_pixelShader->Release();
        m_pixelShader = nullptr;
    }

    if (m_vertexShader) {
        m_vertexShader->Release();
        m_vertexShader = nullptr;
    }
}

void NormalMapShaderClass::OutputShaderErrorMessage(ID3D10Blob* errorMessage, HWND hwnd, const WCHAR* shaderFilename)
{
    char* compileErrors = (char*)(errorMessage->GetBufferPointer());
    unsigned long bufferSize = errorMessage->GetBufferSize();

    std::ofstream fout;
    fout.open("shader-error.txt");
    for (unsigned long i = 0; i < bufferSize; ++i) {
        fout << compileErrors[i];
    }
    fout.close();

    MessageBox(hwnd, L"Error compiling shader. Check shader-error.txt for message.", L"Shader Error", MB_OK);
}

bool NormalMapShaderClass::SetShaderParameters(
    ID3D11DeviceContext* deviceContext,
    XMMATRIX worldMatrix,
    XMMATRIX viewMatrix,
    XMMATRIX projectionMatrix,
    ID3D11ShaderResourceView* texture,
    ID3D11ShaderResourceView* normalMap,
    XMFLOAT3 lightDirection,
    XMFLOAT4 diffuseColor)
{
    HRESULT result;
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    MatrixBufferType* dataPtr;
    LightBufferType* lightPtr;
    unsigned int bufferNumber;

    // Транспонируем матрицы для HLSL
    worldMatrix = XMMatrixTranspose(worldMatrix);
    viewMatrix = XMMatrixTranspose(viewMatrix);
    projectionMatrix = XMMatrixTranspose(projectionMatrix);

    // Заполняем буфер матриц
    result = deviceContext->Map(m_matrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(result)) {
        return false;
    }

    dataPtr = (MatrixBufferType*)(mappedResource.pData);
    dataPtr->world = worldMatrix;
    dataPtr->view = viewMatrix;
    dataPtr->projection = projectionMatrix;

    deviceContext->Unmap(m_matrixBuffer, 0);

    bufferNumber = 0;
    deviceContext->VSSetConstantBuffers(bufferNumber, 1, &m_matrixBuffer);

    // Заполняем буфер света
    result = deviceContext->Map(m_lightBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(result)) {
        return false;
    }

    lightPtr = (LightBufferType*)(mappedResource.pData);
    lightPtr->diffuseColor = diffuseColor;
    lightPtr->lightDirection = lightDirection;
    lightPtr->padding = 0.0f;

    deviceContext->Unmap(m_lightBuffer, 0);

    bufferNumber = 0;
    deviceContext->PSSetConstantBuffers(bufferNumber, 1, &m_lightBuffer);

    // Устанавливаем текстуры
    deviceContext->PSSetShaderResources(0, 1, &texture);
    deviceContext->PSSetShaderResources(1, 1, &normalMap);

    // Устанавливаем сэмплер
    deviceContext->PSSetSamplers(0, 1, &m_sampleState);

    return true;
}

void NormalMapShaderClass::RenderShader(ID3D11DeviceContext* deviceContext, int indexCount)
{
    deviceContext->IASetInputLayout(m_layout);
    deviceContext->VSSetShader(m_vertexShader, NULL, 0);
    deviceContext->PSSetShader(m_pixelShader, NULL, 0);
    deviceContext->DrawIndexed(indexCount, 0, 0);
}
