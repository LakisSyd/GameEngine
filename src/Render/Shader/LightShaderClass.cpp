////////////////////////////////////////////////////////////////////////////////
// Имя файла: lightshaderclass.cpp
////////////////////////////////////////////////////////////////////////////////
// 📌 РЕАЛИЗАЦИЯ: LightShaderClass
// 💡 Загрузка шейдеров, передача данных освещения в GPU
////////////////////////////////////////////////////////////////////////////////

#include "lightshaderclass.h"
#include "helper.h"  // Ваша утилита для проверки HRESULT


const WCHAR* LightPsPath = L"src/Render/Shader/Ps/light.ps";
const WCHAR* LightVsPath = L"src/Render/Shader/Vs/light.vs";

// ─────────────────────────────────────────────────────────────────────────────
// Конструктор
// ─────────────────────────────────────────────────────────────────────────────
LightShaderClass::LightShaderClass()
{
    // Инициализируем только свои поля
    m_matrixBuffer = m_lightBuffer = nullptr;
    m_lightPositionBuffer = m_lightColorBuffer = nullptr;
    m_selectedBuffer = nullptr;
    // Поля базового класса (m_vertexShader и т.д.) обнулятся в его конструкторе
}

void LightShaderClass::Shutdown()
{
    ShutdownShader();           // Твой метод очистки буферов
    ShaderBaseClass::Shutdown(); // Очистка базового класса
}


LightShaderClass::~LightShaderClass()
{
    Shutdown();
}
bool LightShaderClass::Initialize(ID3D11Device* device, HWND hwnd)
{
    // Твоя старая логика инициализации
    // Убедись, что внутри вызывается InitializeShader
    return InitializeShader(device, hwnd, LightVsPath, LightPsPath);
}

// ─────────────────────────────────────────────────────────────────────────────
// Инициализация шейдеров
// ─────────────────────────────────────────────────────────────────────────────
bool LightShaderClass::InitializeShader(
    ID3D11Device* device, HWND hwnd,
    const WCHAR* vsFilename, const WCHAR* psFilename)
{
    ID3D10Blob* vsBlob = nullptr, * psBlob = nullptr;
    HRESULT result;

    // 1. Компиляция и создание шейдеров (базовый класс)
    if (!CompileShader(vsFilename, "LightVertexShader", "vs_5_0", &vsBlob)) return false;

    // 🔥 РЕАЛЬНЫЙ INPUT LAYOUT (замени заглушку на это):
    D3D11_INPUT_ELEMENT_DESC polygonLayout[3];

    polygonLayout[0].SemanticName = "POSITION";
    polygonLayout[0].SemanticIndex = 0;
    polygonLayout[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    polygonLayout[0].InputSlot = 0;
    polygonLayout[0].AlignedByteOffset = 0;
    polygonLayout[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
    polygonLayout[0].InstanceDataStepRate = 0;

    polygonLayout[1].SemanticName = "TEXCOORD";
    polygonLayout[1].SemanticIndex = 0;
    polygonLayout[1].Format = DXGI_FORMAT_R32G32_FLOAT;
    polygonLayout[1].InputSlot = 0;
    polygonLayout[1].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
    polygonLayout[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
    polygonLayout[1].InstanceDataStepRate = 0;

    polygonLayout[2].SemanticName = "NORMAL";
    polygonLayout[2].SemanticIndex = 0;
    polygonLayout[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    polygonLayout[2].InputSlot = 0;
    polygonLayout[2].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
    polygonLayout[2].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
    polygonLayout[2].InstanceDataStepRate = 0;

    if (!CreateInputLayout(device, polygonLayout, 3, vsBlob)) return false;

    result = device->CreateVertexShader(
        vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(),
        nullptr, &m_vertexShader);
    if (FAILED(result)) { vsBlob->Release(); if (psBlob) psBlob->Release(); return false; }

    if (!CompileShader(psFilename, "LightPixelShader", "ps_5_0", &psBlob)) {
        vsBlob->Release(); return false;
    }

    result = device->CreatePixelShader(
        psBlob->GetBufferPointer(),
        psBlob->GetBufferSize(),
        nullptr, &m_pixelShader);
    if (FAILED(result)) { vsBlob->Release(); psBlob->Release(); return false; }

    vsBlob->Release();
    psBlob->Release();

    // 2. Общие ресурсы (базовый класс)
    if (!CreateDefaultSampler(device)) return false;

    // 3. Уникальные буферы (хелпер базового класса)
    if (!CreateConstantBuffer(device, sizeof(MatrixBufferType), &m_matrixBuffer)) return false;
    if (!CreateConstantBuffer(device, sizeof(LightPositionBufferType), &m_lightPositionBuffer)) return false;
    if (!CreateConstantBuffer(device, sizeof(LightColorBufferType), &m_lightColorBuffer)) return false;
    if (!CreateConstantBuffer(device, sizeof(SelectedBufferType), &m_selectedBuffer)) return false;

    return true;
}
// Освобождение ресурсов

// ─────────────────────────────────────────────────────────────────────────────
// Рендеринг с освещением
// ─────────────────────────────────────────────────────────────────────────────
// 🔥 ВОТ ПРАВИЛЬНАЯ СТРОКА (скопируй её целиком):
bool LightShaderClass::Render(
    ID3D11DeviceContext* deviceContext,
    int indexCount,
    XMMATRIX worldMatrix,
    XMMATRIX viewMatrix,
    XMMATRIX projectionMatrix,
    ID3D11ShaderResourceView* texture,
    XMFLOAT4 lightColors[],
    XMFLOAT4 lightPositions[])
{
    bool result;

    // Вызов внутренней функции настройки параметров
    result = SetShaderParameters(
        deviceContext,
        worldMatrix,
        viewMatrix,
        projectionMatrix,
        texture,
        lightColors,
        lightPositions
    );

    if (!result)
    {
        return false;
    }

    // Отрисовка
    RenderShader(deviceContext, indexCount);

    return true;
}
// ─────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────────
// Освобождение шейдеров
// ─────────────────────────────────────────────────────────────────────────────
void LightShaderClass::ShutdownShader()
{

    if (m_lightPositionBuffer) { m_lightPositionBuffer->Release(); m_lightPositionBuffer = nullptr; }
    if (m_lightColorBuffer) { m_lightColorBuffer->Release(); m_lightColorBuffer = nullptr; }

    if (m_lightBuffer)
    {
        m_lightBuffer->Release();
        m_lightBuffer = nullptr;
    }

    if (m_matrixBuffer)
    {
        m_matrixBuffer->Release();
        m_matrixBuffer = nullptr;
    }

    if (m_sampleState)
    {
        m_sampleState->Release();
        m_sampleState = nullptr;
    }

    if (m_layout)
    {
        m_layout->Release();
        m_layout = nullptr;
    }

    if (m_pixelShader)
    {
        m_pixelShader->Release();
        m_pixelShader = nullptr;
    }

    if (m_vertexShader)
    {
        m_vertexShader->Release();
        m_vertexShader = nullptr;
    }
    if (m_selectedBuffer)
    {
        m_selectedBuffer->Release();
        m_selectedBuffer = nullptr;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Вывод ошибки компиляции шейдера
// ─────────────────────────────────────────────────────────────────────────────
void LightShaderClass::OutputShaderErrorMessage(
    ID3D10Blob* errorMessage,
    HWND hwnd,
    WCHAR* shaderFilename)
{
    char* compileErrors;
    unsigned long long bufferSize;
    std::ofstream fout;

    compileErrors = (char*)(errorMessage->GetBufferPointer());
    bufferSize = errorMessage->GetBufferSize();

    fout.open("shader-error.txt");
    for (unsigned long long i = 0; i < bufferSize; i++)
    {
        fout << compileErrors[i];
    }
    fout.close();

    errorMessage->Release();
    errorMessage = nullptr;

    MessageBox(hwnd, L"Ошибка компиляции шейдера! Проверьте shader-error.txt",
        shaderFilename, MB_OK);
}

// ─────────────────────────────────────────────────────────────────────────────
// Установка параметров шейдера
// ─────────────────────────────────────────────────────────────────────────────
bool LightShaderClass::SetShaderParameters(
    ID3D11DeviceContext* deviceContext,
    XMMATRIX worldMatrix, XMMATRIX viewMatrix, XMMATRIX projectionMatrix,
    ID3D11ShaderResourceView* texture,
    XMFLOAT4 lightColors[],
    XMFLOAT4 lightPositions[])
{
    HRESULT result;
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    unsigned int bufferNumber;

    // 1. Матрицы (как обычно)
    worldMatrix = XMMatrixTranspose(worldMatrix);
    viewMatrix = XMMatrixTranspose(viewMatrix);
    projectionMatrix = XMMatrixTranspose(projectionMatrix);

    result = deviceContext->Map(m_matrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(result)) return false;
    MatrixBufferType* dataPtr = (MatrixBufferType*)mappedResource.pData;
    dataPtr->world = worldMatrix;
    dataPtr->view = viewMatrix;
    dataPtr->projection = projectionMatrix;
    deviceContext->Unmap(m_matrixBuffer, 0);
    bufferNumber = 0;
    deviceContext->VSSetConstantBuffers(bufferNumber, 1, &m_matrixBuffer);

    // 2. 🔥 ПЕРЕДАЧА ПОЗИЦИЙ (В ВЕРШИННЫЙ ШЕЙДЕР, слот 1)
    result = deviceContext->Map(m_lightPositionBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(result)) return false;

    LightPositionBufferType* posData = (LightPositionBufferType*)mappedResource.pData;
    // Копируем массив вручную (или memcpy)
    for (int i = 0; i < NUM_LIGHTS; i++) {
        posData->lightPosition[i] = lightPositions[i];
    }

    deviceContext->Unmap(m_lightPositionBuffer, 0);
    bufferNumber = 1; // Слот 1 для позиций
    deviceContext->VSSetConstantBuffers(bufferNumber, 1, &m_lightPositionBuffer);
    // 2а. Также передаём позиции в пиксельный шейдер (слот 1)
    deviceContext->PSSetConstantBuffers(1, 1, &m_lightPositionBuffer);


    // 3. Текстурный ресурс (для пиксельного шейдера)
    deviceContext->PSSetShaderResources(0, 1, &texture);

    // 4. 🔥 ПЕРЕДАЧА ЦВЕТОВ (В ПИКСЕЛЬНЫЙ ШЕЙДЕР, слот 0)
    result = deviceContext->Map(m_lightColorBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(result)) return false;

    LightColorBufferType* colorData = (LightColorBufferType*)mappedResource.pData;
    for (int i = 0; i < NUM_LIGHTS; i++) {
        colorData->diffuseColor[i] = lightColors[i];
    }

    deviceContext->Unmap(m_lightColorBuffer, 0);
    bufferNumber = 0; // Слот 0 для цветов
    deviceContext->PSSetConstantBuffers(bufferNumber, 1, &m_lightColorBuffer);

    return true;
}
// ─────────────────────────────────────────────────────────────────────────────
// Рендеринг шейдера
// ─────────────────────────────────────────────────────────────────────────────
void LightShaderClass::RenderShader(ID3D11DeviceContext* deviceContext, int indexCount)
{
    BindShader(deviceContext);  // ← Утилита базового класса!
    deviceContext->DrawIndexed(indexCount, 0, 0);
}

bool LightShaderClass::SetSelectedFactor(ID3D11DeviceContext* deviceContext, float factor)
{
    HRESULT result;
    D3D11_MAPPED_SUBRESOURCE mappedResource;

    result = deviceContext->Map(m_selectedBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(result)) return false;

    SelectedBufferType* dataPtr = (SelectedBufferType*)mappedResource.pData;
    dataPtr->selectedFactor = factor;
    // padding не трогаем

    deviceContext->Unmap(m_selectedBuffer, 0);

    // Привязываем буфер к пиксельному шейдеру, например, в слоте 2
    unsigned int bufferNumber = 2;
    deviceContext->PSSetConstantBuffers(bufferNumber, 1, &m_selectedBuffer);

    return true;
}