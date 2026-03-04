#include "modelclass.h"
#include "helper.h"


// Конструктор инициализирует указатели буферов.
ModelClass::ModelClass()
{
    m_vertexBuffer = nullptr;
    m_indexBuffer = nullptr;
    m_Texture = nullptr;
}

ModelClass::ModelClass(const ModelClass& other)
{
}

ModelClass::~ModelClass()
{
}

// Initialize создает буферы.
bool ModelClass::Initialize(ID3D11Device* device, ID3D11DeviceContext* deviceContext, const char* textureFilename)
{

     DX_CHECK(InitializeBuffers(device));
   
     DX_CHECK(LoadTexture(device, deviceContext, textureFilename));


     return true;
}

// Shutdown освобождает буферы.
void ModelClass::Shutdown()
{
    ShutdownBuffers();
    ReleaseTexture();
    return;
}
ID3D11ShaderResourceView* ModelClass::GetTexture()
{
    return m_Texture->GetTexture();
}


// Render подготавливает буферы для отрисовки.
void ModelClass::Render(ID3D11DeviceContext* deviceContext)
{
    RenderBuffers(deviceContext);
    return;
}

// Возвращает количество индексов (необходимо шейдеру для отрисовки).
int ModelClass::GetIndexCount()
{
    return m_indexCount;
}

bool ModelClass::InitializeBuffers(ID3D11Device* device)
{
    VertexType* vertices;
    unsigned long* indices;
    D3D11_BUFFER_DESC vertexBufferDesc, indexBufferDesc;
    D3D11_SUBRESOURCE_DATA vertexData, indexData;
    HRESULT result;

    // Устанавливаем количество вершин и индексов.
    // Для квадрата нужно 4 вершины и 6 индексов (2 треугольника)
    m_vertexCount = 3;
    m_indexCount = 3;

    // Создаем массивы
    vertices = new VertexType[m_vertexCount];
    if (!vertices) return false;

    indices = new unsigned long[m_indexCount];
    if (!indices) {
        delete[] vertices;
        return false;
    }

    vertices[0].position = XMFLOAT3(-1.0f, -1.0f, 0.0f);
    vertices[0].texture = XMFLOAT2(0.0f, 1.0f);
    vertices[0].normal = XMFLOAT3(0.0f, 0.0f, -1.0f);  // ← НОРМАЛЬ!

    // Вершина 1: Верхний центр
    vertices[1].position = XMFLOAT3(0.0f, 1.0f, 0.0f);
    vertices[1].texture = XMFLOAT2(0.5f, 0.0f);
    vertices[1].normal = XMFLOAT3(0.0f, 0.0f, -1.0f);  // ← НОРМАЛЬ!

    // Вершина 2: Нижний правый
    vertices[2].position = XMFLOAT3(1.0f, -1.0f, 0.0f);
    vertices[2].texture = XMFLOAT2(1.0f, 1.0f);
    vertices[2].normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
    // === ЗАПОЛНЕНИЕ ИНДЕКСОВ (2 треугольника) ===
    // Треугольник 1: 0, 1, 2
    indices[0] = 0;
    indices[1] = 1;
    indices[2] = 2;
;


    // === НАСТРОЙКА ОПИСАНИЯ ВЕРШИННОГО БУФЕРА ===
    vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    vertexBufferDesc.ByteWidth = sizeof(VertexType) * m_vertexCount;
    vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vertexBufferDesc.CPUAccessFlags = 0;
    vertexBufferDesc.MiscFlags = 0;
    vertexBufferDesc.StructureByteStride = 0;

    // Заполняем структуру данных ресурса
    vertexData.pSysMem = vertices;
    vertexData.SysMemPitch = 0;
    vertexData.SysMemSlicePitch = 0;

    // Создаем вершинный буфер
    result = device->CreateBuffer(&vertexBufferDesc, &vertexData, &m_vertexBuffer);
    if (FAILED(result))
    {
        delete[] vertices;
        delete[] indices;
        return false;
    }

    // === НАСТРОЙКА ОПИСАНИЯ ИНДЕКСНОГО БУФЕРА ===
    indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    indexBufferDesc.ByteWidth = sizeof(unsigned long) * m_indexCount;
    indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    indexBufferDesc.CPUAccessFlags = 0;
    indexBufferDesc.MiscFlags = 0;
    indexBufferDesc.StructureByteStride = 0;

    // Заполняем структуру данных ресурса
    indexData.pSysMem = indices;
    indexData.SysMemPitch = 0;
    indexData.SysMemSlicePitch = 0;

    // Создаем индексный буфер
    result = device->CreateBuffer(&indexBufferDesc, &indexData, &m_indexBuffer);
    if (FAILED(result))
    {
        delete[] vertices;
        delete[] indices;
        return false;
    }

    // Освобождаем временные массивы (данные теперь в видеопамяти)
    delete[] vertices;
    vertices = 0;

    delete[] indices;
    indices = 0;

    return true;
}

void ModelClass::ShutdownBuffers()
{
    if (m_indexBuffer)
    {
        m_indexBuffer->Release();
        m_indexBuffer = 0;
    }
    if (m_vertexBuffer)
    {
        m_vertexBuffer->Release();
        m_vertexBuffer = 0;
    }
    return;
}

void ModelClass::RenderBuffers(ID3D11DeviceContext* deviceContext)
{
    unsigned int stride;
    unsigned int offset;

    // Шаг страйда равен размеру одной вершины.
    stride = sizeof(VertexType);
    offset = 0;

    // Активируем вершинный буфер.
    deviceContext->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);

    // Активируем индексный буфер.
    deviceContext->IASetIndexBuffer(m_indexBuffer, DXGI_FORMAT_R32_UINT, 0);

    // Указываем топологию примитивов: список треугольников.
    deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    return;
}

bool ModelClass::LoadTexture(ID3D11Device* device, ID3D11DeviceContext* deviceContext, const char* filename)
{
    bool result;
    m_Texture = new TextureClass;
    if (!m_Texture) return false;

    result = m_Texture->Initialize(device, deviceContext, filename);
    if (!result)
    {
        // ▼▼▼ ДОБАВЬТЕ ЭТОТ БЛОК ▼▼▼
        MessageBoxA(NULL,
            "Не удалось загрузить текстуру!\nПроверьте наличие файла stone.tga\nв папке с .exe файлом.",
            "Ошибка текстуры", MB_OK | MB_ICONERROR);
        // ▲▲▲ КОНЕЦ БЛОКА ▲▲▲

        delete m_Texture;
        m_Texture = 0;
        return false;
    }
    return true;
}

void ModelClass::ReleaseTexture()
{
    // Освобождаем объект текстуры
    if (m_Texture)
    {
        m_Texture->Shutdown();
        delete m_Texture;
        m_Texture = 0;
    }

    return;
}