#include "modelclass.h"
#include "helper.h"


// Конструктор инициализирует указатели буферов.
ModelClass::ModelClass()
{
    m_vertexBuffer = nullptr;
    m_indexBuffer = nullptr;
    m_Textures[0] = nullptr;  // 🔥 Инициализируем массив
    m_Textures[1] = nullptr;
}

ModelClass::~ModelClass()
{
    Shutdown();
}

// Initialize создает буферы.
bool ModelClass::Initialize(ID3D11Device* device, ID3D11DeviceContext* deviceContext, const char* tex1, const char* tex2)
{

     DX_CHECK(InitializeBuffers(device));
   
     DX_CHECK(LoadTextures(device, deviceContext, tex1, tex2));


     return true;
}

// Shutdown освобождает буферы.
void ModelClass::Shutdown()
{
    ShutdownBuffers();
    ReleaseTextures();
    return;
}
ID3D11ShaderResourceView* ModelClass::GetTexture(int index)
{
    if (index < 0 || index >= 2) return nullptr;
    return m_Textures[index] ? m_Textures[index]->GetTexture() : nullptr;
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

bool ModelClass::LoadTextures(ID3D11Device* device, ID3D11DeviceContext* deviceContext,
    const char* filename1, const char* filename2)
{
    // Текстура 0: альбедо
    m_Textures[0] = new TextureClass();
    if (!m_Textures[0]->Initialize(device, deviceContext, filename1)) {
        delete m_Textures[0]; m_Textures[0] = nullptr;
        return false;
    }

    // Текстура 1: детали/нормали
    m_Textures[1] = new TextureClass();
    if (!m_Textures[1]->Initialize(device, deviceContext, filename2)) {
        delete m_Textures[1]; m_Textures[1] = nullptr;
        return false;
    }
    return true;
}
void ModelClass::ReleaseTextures()
{
    for (int i = 0; i < 2; ++i) {
        if (m_Textures[i]) {
            m_Textures[i]->Shutdown();
            delete m_Textures[i];
            m_Textures[i] = nullptr;
        }
    }
}
