#include "SkyboxClass.h"
#include "helper.h"

// Конструктор инициализирует указатели буферов.
SkyboxClass::SkyboxClass()
{
    m_vertexBuffer = nullptr;
    m_indexBuffer = nullptr;
    for (int i = 0; i < 6; ++i) {
        m_Textures[i] = nullptr;
    }
}

SkyboxClass::SkyboxClass(const SkyboxClass& other)
{
}

SkyboxClass::~SkyboxClass()
{
}

// Initialize создает буферы и загружает текстуры.
bool SkyboxClass::Initialize(ID3D11Device* device, ID3D11DeviceContext* deviceContext,
                             const char* right, const char* left,
                             const char* top, const char* bottom,
                             const char* front, const char* back)
{
    DX_CHECK(InitializeBuffers(device));
    DX_CHECK(LoadTextures(device, deviceContext, right, left, top, bottom, front, back));
    return true;
}

// Shutdown освобождает буферы и текстуры.
void SkyboxClass::Shutdown()
{
    ShutdownBuffers();
    ReleaseTextures();
}

ID3D11ShaderResourceView* SkyboxClass::GetTexture(int index)
{
    if (index < 0 || index >= 6) return nullptr;
    return m_Textures[index] ? m_Textures[index]->GetTexture() : nullptr;
}

// Render подготавливает буферы для отрисовки.
void SkyboxClass::Render(ID3D11DeviceContext* deviceContext)
{
    RenderBuffers(deviceContext);
}

bool SkyboxClass::InitializeBuffers(ID3D11Device* device)
{
    VertexType* vertices;
    unsigned long* indices;
    D3D11_BUFFER_DESC vertexBufferDesc, indexBufferDesc;
    D3D11_SUBRESOURCE_DATA vertexData, indexData;
    HRESULT result;

    // Создаем куб (skybox) - 24 вершины (4 на каждую из 6 граней)
    m_vertexCount = 24;
    m_indexCount = 36;  // 6 граней * 2 треугольника * 3 индекса = 36

    vertices = new VertexType[m_vertexCount];
    if (!vertices) return false;

    indices = new unsigned long[m_indexCount];
    if (!indices) {
        delete[] vertices;
        return false;
    }

    // Вершины для каждой грани куба
    // Грани: Right(+X), Left(-X), Top(+Y), Bottom(-Y), Front(+Z), Back(-Z)
    
    // Right face (+X) - вершины 0-3
    vertices[0].position = XMFLOAT3(1.0f, -1.0f, -1.0f);  vertices[0].texture = XMFLOAT2(0.0f, 1.0f);
    vertices[1].position = XMFLOAT3(1.0f, 1.0f, -1.0f);   vertices[1].texture = XMFLOAT2(0.0f, 0.0f);
    vertices[2].position = XMFLOAT3(1.0f, 1.0f, 1.0f);    vertices[2].texture = XMFLOAT2(1.0f, 0.0f);
    vertices[3].position = XMFLOAT3(1.0f, -1.0f, 1.0f);   vertices[3].texture = XMFLOAT2(1.0f, 1.0f);

    // Left face (-X) - вершины 4-7
    vertices[4].position = XMFLOAT3(-1.0f, -1.0f, 1.0f);  vertices[4].texture = XMFLOAT2(0.0f, 1.0f);
    vertices[5].position = XMFLOAT3(-1.0f, 1.0f, 1.0f);   vertices[5].texture = XMFLOAT2(0.0f, 0.0f);
    vertices[6].position = XMFLOAT3(-1.0f, 1.0f, -1.0f);  vertices[6].texture = XMFLOAT2(1.0f, 0.0f);
    vertices[7].position = XMFLOAT3(-1.0f, -1.0f, -1.0f); vertices[7].texture = XMFLOAT2(1.0f, 1.0f);

    // Top face (+Y) - вершины 8-11
    vertices[8].position = XMFLOAT3(-1.0f, 1.0f, -1.0f);  vertices[8].texture = XMFLOAT2(0.0f, 1.0f);
    vertices[9].position = XMFLOAT3(-1.0f, 1.0f, 1.0f);   vertices[9].texture = XMFLOAT2(0.0f, 0.0f);
    vertices[10].position = XMFLOAT3(1.0f, 1.0f, 1.0f);   vertices[10].texture = XMFLOAT2(1.0f, 0.0f);
    vertices[11].position = XMFLOAT3(1.0f, 1.0f, -1.0f);  vertices[11].texture = XMFLOAT2(1.0f, 1.0f);

    // Bottom face (-Y) - вершины 12-15
    vertices[12].position = XMFLOAT3(-1.0f, -1.0f, 1.0f);  vertices[12].texture = XMFLOAT2(0.0f, 1.0f);
    vertices[13].position = XMFLOAT3(-1.0f, -1.0f, -1.0f); vertices[13].texture = XMFLOAT2(0.0f, 0.0f);
    vertices[14].position = XMFLOAT3(1.0f, -1.0f, -1.0f);  vertices[14].texture = XMFLOAT2(1.0f, 0.0f);
    vertices[15].position = XMFLOAT3(1.0f, -1.0f, 1.0f);   vertices[15].texture = XMFLOAT2(1.0f, 1.0f);

    // Front face (+Z) - вершины 16-19
    vertices[16].position = XMFLOAT3(-1.0f, -1.0f, 1.0f);  vertices[16].texture = XMFLOAT2(0.0f, 1.0f);
    vertices[17].position = XMFLOAT3(-1.0f, 1.0f, 1.0f);   vertices[17].texture = XMFLOAT2(0.0f, 0.0f);
    vertices[18].position = XMFLOAT3(1.0f, 1.0f, 1.0f);    vertices[18].texture = XMFLOAT2(1.0f, 0.0f);
    vertices[19].position = XMFLOAT3(1.0f, -1.0f, 1.0f);   vertices[19].texture = XMFLOAT2(1.0f, 1.0f);

    // Back face (-Z) - вершины 20-23
    vertices[20].position = XMFLOAT3(1.0f, -1.0f, -1.0f);  vertices[20].texture = XMFLOAT2(0.0f, 1.0f);
    vertices[21].position = XMFLOAT3(1.0f, 1.0f, -1.0f);   vertices[21].texture = XMFLOAT2(0.0f, 0.0f);
    vertices[22].position = XMFLOAT3(-1.0f, 1.0f, -1.0f);  vertices[22].texture = XMFLOAT2(1.0f, 0.0f);
    vertices[23].position = XMFLOAT3(-1.0f, -1.0f, -1.0f); vertices[23].texture = XMFLOAT2(1.0f, 1.0f);

    // Индексы для каждой грани (2 треугольника на грань)
    // Right
    indices[0] = 0; indices[1] = 1; indices[2] = 2;
    indices[3] = 0; indices[4] = 2; indices[5] = 3;
    // Left
    indices[6] = 4; indices[7] = 5; indices[8] = 6;
    indices[9] = 4; indices[10] = 6; indices[11] = 7;
    // Top
    indices[12] = 8; indices[13] = 9; indices[14] = 10;
    indices[15] = 8; indices[16] = 10; indices[17] = 11;
    // Bottom
    indices[18] = 12; indices[19] = 13; indices[20] = 14;
    indices[21] = 12; indices[22] = 14; indices[23] = 15;
    // Front
    indices[24] = 16; indices[25] = 17; indices[26] = 18;
    indices[27] = 16; indices[28] = 18; indices[29] = 19;
    // Back
    indices[30] = 20; indices[31] = 21; indices[32] = 22;
    indices[33] = 20; indices[34] = 22; indices[35] = 23;

    // Настройка вершинного буфера
    vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    vertexBufferDesc.ByteWidth = sizeof(VertexType) * m_vertexCount;
    vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vertexBufferDesc.CPUAccessFlags = 0;
    vertexBufferDesc.MiscFlags = 0;
    vertexBufferDesc.StructureByteStride = 0;

    vertexData.pSysMem = vertices;
    vertexData.SysMemPitch = 0;
    vertexData.SysMemSlicePitch = 0;

    result = device->CreateBuffer(&vertexBufferDesc, &vertexData, &m_vertexBuffer);
    if (FAILED(result))
    {
        delete[] vertices;
        delete[] indices;
        return false;
    }

    // Настройка индексного буфера
    indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    indexBufferDesc.ByteWidth = sizeof(unsigned long) * m_indexCount;
    indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    indexBufferDesc.CPUAccessFlags = 0;
    indexBufferDesc.MiscFlags = 0;
    indexBufferDesc.StructureByteStride = 0;

    indexData.pSysMem = indices;
    indexData.SysMemPitch = 0;
    indexData.SysMemSlicePitch = 0;

    result = device->CreateBuffer(&indexBufferDesc, &indexData, &m_indexBuffer);
    if (FAILED(result))
    {
        delete[] vertices;
        delete[] indices;
        return false;
    }

    delete[] vertices;
    delete[] indices;

    return true;
}

void SkyboxClass::ShutdownBuffers()
{
    if (m_indexBuffer)
    {
        m_indexBuffer->Release();
        m_indexBuffer = nullptr;
    }
    if (m_vertexBuffer)
    {
        m_vertexBuffer->Release();
        m_vertexBuffer = nullptr;
    }
}

void SkyboxClass::RenderBuffers(ID3D11DeviceContext* deviceContext)
{
    unsigned int stride = sizeof(VertexType);
    unsigned int offset = 0;

    deviceContext->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);
    deviceContext->IASetIndexBuffer(m_indexBuffer, DXGI_FORMAT_R32_UINT, 0);
    deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

bool SkyboxClass::LoadTextures(ID3D11Device* device, ID3D11DeviceContext* deviceContext,
                               const char* right, const char* left,
                               const char* top, const char* bottom,
                               const char* front, const char* back)
{
    const char* filenames[6] = { right, left, top, bottom, front, back };

    for (int i = 0; i < 6; ++i) {
        m_Textures[i] = new TextureClass();
        if (!m_Textures[i]->Initialize(device, deviceContext, filenames[i])) {
            delete m_Textures[i];
            m_Textures[i] = nullptr;
            return false;
        }
    }
    return true;
}

void SkyboxClass::ReleaseTextures()
{
    for (int i = 0; i < 6; ++i) {
        if (m_Textures[i]) {
            m_Textures[i]->Shutdown();
            delete m_Textures[i];
            m_Textures[i] = nullptr;
        }
    }
}
