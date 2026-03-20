////////////////////////////////////////////////////////////////////////////////
// Filename: clipplaneshaderclass.cpp
////////////////////////////////////////////////////////////////////////////////

#include "clipplaneshaderclass.h"

ClipPlaneShaderClass::ClipPlaneShaderClass()
{
    m_vertexShader = nullptr;
    m_pixelShader = nullptr;
    m_layout = nullptr;
    m_matrixBuffer = nullptr;
    m_sampleState = nullptr;

    // Инициализируем указатель буфера плоскости отсечения значением null в конструкторе класса.
    m_clipPlaneBuffer = nullptr;
}

ClipPlaneShaderClass::ClipPlaneShaderClass(const ClipPlaneShaderClass& other)
{
}

ClipPlaneShaderClass::~ClipPlaneShaderClass()
{
}

bool ClipPlaneShaderClass::Initialize(ID3D11Device* device, HWND hwnd)
{
    bool result;
    wchar_t vsFilename[128];
    wchar_t psFilename[128];
    int error;

    // Задаем имя файла вершинного шейдера.
    error = wcscpy_s(vsFilename, 128, L"src/Render/Shader/Vs/clipplane.vs");
    if (error != 0)
    {
        return false;
    }

    // Задаем имя файла пиксельного шейдера.
    error = wcscpy_s(psFilename, 128, L"src/Render/Shader/Ps/clipplane.ps");
    if (error != 0)
    {
        return false;
    }

    // Инициализируем вершинный и пиксельный шейдеры.
    result = InitializeShader(device, hwnd, vsFilename, psFilename);
    if (!result)
    {
        return false;
    }

    return true;
}

void ClipPlaneShaderClass::Shutdown()
{
    // Завершаем работу вершинного и пиксельного шейдеров, а также связанных с ними объектов.
    ShutdownShader();
    return;
}

// Функция Render теперь также принимает на вход вектор плоскости отсечения из 4-х float. 
// Эта плоскость отсечения затем устанавливается в шейдере перед рендерингом.
bool ClipPlaneShaderClass::Render(ID3D11DeviceContext* deviceContext, int indexCount, XMMATRIX worldMatrix, XMMATRIX viewMatrix,
    XMMATRIX projectionMatrix, ID3D11ShaderResourceView* texture, XMFLOAT4 clipPlane)
{
    bool result;

    // Устанавливаем параметры шейдера, которые будут использоваться для рендеринга.
    result = SetShaderParameters(deviceContext, worldMatrix, viewMatrix, projectionMatrix, texture, clipPlane);
    if (!result)
    {
        return false;
    }

    // Теперь отрисовываем подготовленные буферы с помощью шейдера.
    RenderShader(deviceContext, indexCount);

    return true;
}

bool ClipPlaneShaderClass::InitializeShader(ID3D11Device* device, HWND hwnd, WCHAR* vsFilename, WCHAR* psFilename)
{
    HRESULT result;
    ID3D10Blob* errorMessage;
    ID3D10Blob* vertexShaderBuffer;
    ID3D10Blob* pixelShaderBuffer;
    D3D11_INPUT_ELEMENT_DESC polygonLayout[2];
    unsigned int numElements;
    D3D11_BUFFER_DESC matrixBufferDesc;
    D3D11_SAMPLER_DESC samplerDesc;
    D3D11_BUFFER_DESC clipPlaneBufferDesc;

    // Инициализируем указатели, которые будет использовать эта функция, значением null.
    errorMessage = 0;
    vertexShaderBuffer = 0;
    pixelShaderBuffer = 0;

    // Компилируем код вершинного шейдера.
    result = D3DCompileFromFile(vsFilename, NULL, NULL, "ClipPlaneVertexShader", "vs_5_0", D3D10_SHADER_ENABLE_STRICTNESS, 0,
        &vertexShaderBuffer, &errorMessage);
    if (FAILED(result))
    {
        // Если шейдер не удалось скомпилировать, в сообщении об ошибке должно быть что-то написано.
        if (errorMessage)
        {
            OutputShaderErrorMessage(errorMessage, hwnd, vsFilename);
        }
        // Если в сообщении об ошибке ничего не указано, значит, файл шейдера просто не найден.
        else
        {
            MessageBox(hwnd, vsFilename, L"Missing Shader File", MB_OK);
        }
        return false;
    }

    // Компилируем код пиксельного шейдера.
    result = D3DCompileFromFile(psFilename, NULL, NULL, "ClipPlanePixelShader", "ps_5_0", D3D10_SHADER_ENABLE_STRICTNESS, 0,
        &pixelShaderBuffer, &errorMessage);
    if (FAILED(result))
    {
        // Если шейдер не удалось скомпилировать, в сообщении об ошибке должно быть что-то написано.
        if (errorMessage)
        {
            OutputShaderErrorMessage(errorMessage, hwnd, psFilename);
        }
        // Если в сообщении об ошибке ничего не указано, значит, файл просто не найден.
        else
        {
            MessageBox(hwnd, psFilename, L"Missing Shader File", MB_OK);
        }
        return false;
    }

    // Создаем вершинный шейдер из буфера.
    result = device->CreateVertexShader(vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), NULL, &m_vertexShader);
    if (FAILED(result))
    {
        return false;
    }

    // Создаем пиксельный шейдер из буфера.
    result = device->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(), pixelShaderBuffer->GetBufferSize(), NULL, &m_pixelShader);
    if (FAILED(result))
    {
        return false;
    }

    // Создаем описание макета входных данных вершин.
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

    // Получаем количество элементов в макете.
    numElements = sizeof(polygonLayout) / sizeof(polygonLayout[0]);

    // Создаем макет входных данных вершин.
    result = device->CreateInputLayout(polygonLayout, numElements, vertexShaderBuffer->GetBufferPointer(),
        vertexShaderBuffer->GetBufferSize(), &m_layout);
    if (FAILED(result))
    {
        return false;
    }

    // Освобождаем буфер вершинного шейдера и буфер пиксельного шейдера, так как они больше не нужны.
    vertexShaderBuffer->Release();
    vertexShaderBuffer = 0;
    pixelShaderBuffer->Release();
    pixelShaderBuffer = 0;

    // Настраиваем описание динамического буфера констант для матриц в вершинном шейдере.
    matrixBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    matrixBufferDesc.ByteWidth = sizeof(MatrixBufferType);
    matrixBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    matrixBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    matrixBufferDesc.MiscFlags = 0;
    matrixBufferDesc.StructureByteStride = 0;

    // Создаем указатель на буфер констант, чтобы мы могли обращаться к нему из этого класса.
    result = device->CreateBuffer(&matrixBufferDesc, NULL, &m_matrixBuffer);
    if (FAILED(result))
    {
        return false;
    }

    // Создаем описание состояния сэмплера текстуры.
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

    // Создаем состояние сэмплера текстуры.
    result = device->CreateSamplerState(&samplerDesc, &m_sampleState);
    if (FAILED(result))
    {
        return false;
    }

    // Настройка буфера плоскости отсечения.

    // Настраиваем описание динамического буфера констант для плоскости отсечения в вершинном шейдере.
    clipPlaneBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    clipPlaneBufferDesc.ByteWidth = sizeof(ClipPlaneBufferType);
    clipPlaneBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    clipPlaneBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    clipPlaneBufferDesc.MiscFlags = 0;
    clipPlaneBufferDesc.StructureByteStride = 0;

    // Создаем указатель на буфер констант, чтобы мы могли обращаться к нему из этого класса.
    result = device->CreateBuffer(&clipPlaneBufferDesc, NULL, &m_clipPlaneBuffer);
    if (FAILED(result))
    {
        return false;
    }

    return true;
}

void ClipPlaneShaderClass::ShutdownShader()
{
    // Освобождаем новый буфер плоскости отсечения в функции ShutdownShader.

    // Освобождаем буфер констант плоскости отсечения.
    if (m_clipPlaneBuffer)
    {
        m_clipPlaneBuffer->Release();
        m_clipPlaneBuffer = 0;
    }

    // Освобождаем состояние сэмплера.
    if (m_sampleState)
    {
        m_sampleState->Release();
        m_sampleState = 0;
    }

    // Освобождаем буфер констант матрицы.
    if (m_matrixBuffer)
    {
        m_matrixBuffer->Release();
        m_matrixBuffer = 0;
    }

    // Освобождаем макет (layout).
    if (m_layout)
    {
        m_layout->Release();
        m_layout = 0;
    }

    // Освобождаем пиксельный шейдер.
    if (m_pixelShader)
    {
        m_pixelShader->Release();
        m_pixelShader = 0;
    }

    // Освобождаем вершинный шейдер.
    if (m_vertexShader)
    {
        m_vertexShader->Release();
        m_vertexShader = 0;
    }

    return;
}

void ClipPlaneShaderClass::OutputShaderErrorMessage(ID3D10Blob* errorMessage, HWND hwnd, WCHAR* shaderFilename)
{
    char* compileErrors;
    unsigned long long bufferSize, i;
    ofstream fout;

    // Получаем указатель на текстовый буфер с сообщением об ошибке.
    compileErrors = (char*)(errorMessage->GetBufferPointer());
    // Получаем длину сообщения.
    bufferSize = errorMessage->GetBufferSize();

    // Открываем файл для записи сообщения об ошибке.
    fout.open("shader-error.txt");

    // Записываем сообщение об ошибке в файл.
    for (i = 0; i < bufferSize; i++)
    {
        fout << compileErrors[i];
    }

    // Закрываем файл.
    fout.close();

    // Освобождаем буфер с сообщением об ошибке.
    errorMessage->Release();
    errorMessage = 0;

    // Выводим сообщение на экран, чтобы пользователь проверил текстовый файл на наличие ошибок компиляции.
    MessageBox(hwnd, L"Error compiling shader. Check shader-error.txt for message.", shaderFilename, MB_OK);

    return;
}

bool ClipPlaneShaderClass::SetShaderParameters(ID3D11DeviceContext* deviceContext, XMMATRIX worldMatrix, XMMATRIX viewMatrix,
    XMMATRIX projectionMatrix, ID3D11ShaderResourceView* texture, XMFLOAT4 clipPlane)
{
    HRESULT result;
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    MatrixBufferType* dataPtr;
    unsigned int bufferNumber;
    ClipPlaneBufferType* dataPtr2;

    // Транспонируем матрицы, чтобы подготовить их для шейдера.
    worldMatrix = XMMatrixTranspose(worldMatrix);
    viewMatrix = XMMatrixTranspose(viewMatrix);
    projectionMatrix = XMMatrixTranspose(projectionMatrix);

    // Блокируем (Map) буфер констант, чтобы в него можно было записывать данные.
    result = deviceContext->Map(m_matrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(result))
    {
        return false;
    }

    // Получаем указатель на данные в постоянном буфере.
    dataPtr = (MatrixBufferType*)mappedResource.pData;

    // Копируем матрицы в постоянный буфер.
    dataPtr->world = worldMatrix;
    dataPtr->view = viewMatrix;
    dataPtr->projection = projectionMatrix;

    // Разблокируем (Unmap) постоянный буфер.
    deviceContext->Unmap(m_matrixBuffer, 0);

    // Устанавливаем индекс постоянного буфера в вершинном шейдере (слот 0).
    bufferNumber = 0;
    // Наконец, устанавливаем постоянный буфер в вершинном шейдере с обновленными значениями.
    deviceContext->VSSetConstantBuffers(bufferNumber, 1, &m_matrixBuffer);

    // Устанавливаем ресурс шейдерной текстуры в пиксельном шейдере.
    deviceContext->PSSetShaderResources(0, 1, &texture);

    // Здесь мы блокируем буфер констант плоскости отсечения, устанавливаем значение плоскости отсечения, 
    // а затем снова разблокируем буфер. Таким образом мы передаем данные плоскости отсечения внутрь шейдера. 
    // Также обратите внимание, что это второй буфер в вершинном шейдере, поэтому нам нужно увеличить 
    // значение bufferNumber с 0 до 1.

    // Блокируем буфер констант плоскости отсечения для записи.
    result = deviceContext->Map(m_clipPlaneBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(result))
    {
        return false;
    }

    // Получаем указатель на данные в буфере констант плоскости отсечения.
    dataPtr2 = (ClipPlaneBufferType*)mappedResource.pData;

    // Копируем плоскость отсечения в буфер констант.
    dataPtr2->clipPlane = clipPlane;

    // Разблокируем буфер.
    deviceContext->Unmap(m_clipPlaneBuffer, 0);

    // Устанавливаем индекс буфера констант плоскости отсечения в вершинном шейдере (слот 1).
    bufferNumber = 1;
    // Теперь устанавливаем буфер констант плоскости отсечения в вершинном шейдере с обновленными значениями.
    deviceContext->VSSetConstantBuffers(bufferNumber, 1, &m_clipPlaneBuffer);

    return true;
}

void ClipPlaneShaderClass::RenderShader(ID3D11DeviceContext* deviceContext, int indexCount)
{
    // Устанавливаем макет входных данных вершин.
    deviceContext->IASetInputLayout(m_layout);

    // Устанавливаем вершинный и пиксельный шейдеры, которые будут использоваться для отрисовки.
    deviceContext->VSSetShader(m_vertexShader, NULL, 0);
    deviceContext->PSSetShader(m_pixelShader, NULL, 0);

    // Устанавливаем состояние сэмплера в пиксельном шейдере.
    deviceContext->PSSetSamplers(0, 1, &m_sampleState);

    // Отрисовываем геометрию.
    deviceContext->DrawIndexed(indexCount, 0, 0);

    return;
}