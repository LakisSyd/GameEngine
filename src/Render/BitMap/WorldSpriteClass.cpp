#include "WorldSpriteClass.h"

WorldSpriteClass::WorldSpriteClass()
{
    m_Texture = nullptr;
    m_vertexBuffer = nullptr;
    m_indexBuffer = nullptr;
    m_position = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_vertexCount = 0;
    m_indexCount = 0;
    m_width = 1.0f;
    m_height = 1.0f;
}

WorldSpriteClass::~WorldSpriteClass()
{
	Shutdown();
}

void WorldSpriteClass::Shutdown()
{
    if (m_Texture)
    {
        m_Texture->Shutdown();  // Важно: сначала Shutdown, потом delete
        delete m_Texture;
        m_Texture = nullptr;
    }

    // 2. Освобождаем вершинный буфер
    if (m_vertexBuffer)
    {
        m_vertexBuffer->Release();
        m_vertexBuffer = nullptr;
    }

    // 3. Освобождаем индексный буфер
    if (m_indexBuffer)
    {
        m_indexBuffer->Release();
        m_indexBuffer = nullptr;
    }

    // Сбрасываем счётчики (опционально, но полезно для отладки)
    m_vertexCount = 0;
    m_indexCount = 0;

    return;
}

bool WorldSpriteClass::Initialize(ID3D11Device* device, ID3D11DeviceContext* deviceContext,const char* textureFilename, float width, float height)
{
    bool result;
    m_width = width;
    m_height = height;

    m_Texture = new TextureClass;

   result = m_Texture->Initialize(device, deviceContext, textureFilename);



   if (!m_Texture->GetTexture())
   {
       OutputDebugStringA("ERROR: Texture not loaded!\n");
       return false;
   }

   if (!result)
   {
       return false;
   }
   result = InitializeBuffers(device, m_width, m_height);
   if (!result)
   {
       // Если буферы не создались — чистим текстуру и возвращаем ошибку
       if (m_Texture)
       {
           m_Texture->Shutdown();
           delete m_Texture;
           m_Texture = nullptr;
       }
       return false;
   }

   return true;
}

bool WorldSpriteClass::Render(
    ID3D11DeviceContext* context,
    XMMATRIX view,
    XMMATRIX projection,
    TextureShaderClass* shader,
    XMMATRIX* customWorldMatrix)  // = nullptr по умолчанию в заголовке
{
    // ==========================================
    // 1. Определяем, какую матрицу мира использовать
    // ==========================================
    XMMATRIX worldMatrix;

    if (customWorldMatrix != nullptr) {
        // 🟢 Caller передал кастомную матрицу — используем её
        worldMatrix = *customWorldMatrix;
    }
    else {
        // 🔵 Нет кастомной матрицы — строим дефолтную из внутренних данных
        XMMATRIX scaleMatrix = XMMatrixScaling(m_width, m_height, 1.0f);
        XMMATRIX transMatrix = XMMatrixTranslation(m_position.x, m_position.y, m_position.z);
        worldMatrix = scaleMatrix * transMatrix; // Порядок: Scale → Translate
    }

    // ==========================================
    // 2. Привязываем буферы и рендерим
    // ==========================================
    RenderBuffers(context);

    return shader->Render(
        context,
        m_indexCount,
        worldMatrix,  // 👈 Теперь переменная определена всегда
        view,
        projection,
        m_Texture->GetTexture()
    );
}

bool WorldSpriteClass::InitializeBuffers(ID3D11Device* device, float width, float height)
{
    VertexType* vertices = nullptr;
    unsigned long* indices = nullptr;
    D3D11_BUFFER_DESC vertexBufferDesc, indexBufferDesc;
    D3D11_SUBRESOURCE_DATA vertexData, indexData;
    HRESULT result;

    m_vertexCount = 6;
    m_indexCount = 6;

    vertices = new VertexType[m_vertexCount];
    if (!vertices) return false;

    float halfW = width / 2.0f;
    float halfH = height / 2.0f;

    halfW *= 10.0f;
    halfH *= 10.0f;

    // === Чёткая схема: каждый треугольник отдельно ===

    // Треугольник 1 (левый нижний)
    // Вершина 0: Top-Left
    vertices[0].position = XMFLOAT3(-halfW, halfH, 0.0f);
    vertices[0].texture = XMFLOAT2(0.0f, 0.0f);

    // Вершина 1: Bottom-Right  
    vertices[1].position = XMFLOAT3(halfW, -halfH, 0.0f);
    vertices[1].texture = XMFLOAT2(1.0f, 1.0f);

    // Вершина 2: Bottom-Left
    vertices[2].position = XMFLOAT3(-halfW, -halfH, 0.0f);
    vertices[2].texture = XMFLOAT2(0.0f, 1.0f);

    // Треугольник 2 (правый верхний)
    // Вершина 3: Top-Left (повтор)
    vertices[3].position = XMFLOAT3(-halfW, halfH, 0.0f);
    vertices[3].texture = XMFLOAT2(0.0f, 0.0f);

    // Вершина 4: Top-Right
    vertices[4].position = XMFLOAT3(halfW, halfH, 0.0f);
    vertices[4].texture = XMFLOAT2(1.0f, 0.0f);

    // Вершина 5: Bottom-Right (повтор)
    vertices[5].position = XMFLOAT3(halfW, -halfH, 0.0f);
    vertices[5].texture = XMFLOAT2(1.0f, 1.0f);

    // Создаём вершинный буфер
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
    if (FAILED(result)) { delete[] vertices; return false; }

    // Создаём индексный буфер (простой последовательный)
    indices = new unsigned long[m_indexCount];
    for (int i = 0; i < m_indexCount; i++) { indices[i] = i; }

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
    if (FAILED(result)) {
        delete[] vertices;
        delete[] indices;
        return false;
    }

    delete[] vertices;
    delete[] indices;
    return true;
}

void WorldSpriteClass::Move(float dx, float dy, float dz)
{
    m_position.x += dx;
    m_position.y += dy;
    m_position.z += dz;
    MarkMatrixDirty(); // Не забываем пометить матрицу!
}

void WorldSpriteClass::RenderBuffers(ID3D11DeviceContext* deviceContext)
{
    unsigned int stride = sizeof(VertexType);
    unsigned int offset = 0;

    // Привязываем вершинный буфер
    deviceContext->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);

    // Привязываем индексный буфер
    deviceContext->IASetIndexBuffer(m_indexBuffer, DXGI_FORMAT_R32_UINT, 0);

    // Указываем топологию: список треугольников
    deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Рисуем!
  //  deviceContext->DrawIndexed(m_indexCount, 0, 0);
}

void WorldSpriteClass::SetPosition(float x, float y, float z)
{
    if (m_position.x != x || m_position.y != y || m_position.z != z) {
        m_position = { x, y, z };
        MarkMatrixDirty();
    }
}
void WorldSpriteClass::SetScale(float width, float height)
{
    if (m_width != width || m_height != height) {
        m_width = width;
        m_height = height;
        MarkMatrixDirty();
    }
}

void WorldSpriteClass::SetRotation(float radians)
{
    if (m_rotation != radians) {
        m_rotation = radians;
        MarkMatrixDirty();
    }
}
void WorldSpriteClass::AddRotation(float radians)
{
    m_rotation += radians;
    MarkMatrixDirty();
}

void WorldSpriteClass::SetTransform(float x, float y, float z,
    float width, float height,
    float rotationRad)
{
    bool changed = false;
    if (m_position.x != x || m_position.y != y || m_position.z != z) {
        m_position = { x, y, z };
        changed = true;
    }
    if (m_width != width || m_height != height) {
        m_width = width;
        m_height = height;
        changed = true;
    }
    if (m_rotation != rotationRad) {
        m_rotation = rotationRad;
        changed = true;
    }
    if (changed) MarkMatrixDirty();
}

// --- Построение матрицы с кэшированием ---
XMMATRIX WorldSpriteClass::BuildWorldMatrix() const
{
    // Если матрица уже посчитана и не менялась — возвращаем кэш
    if (!m_isMatrixDirty) {
        return m_cachedWorldMatrix;
    }

    // Порядок трансформаций: Scale → Rotate → Translate
    XMMATRIX scale = XMMatrixScaling(m_width, m_height, 1.0f);

    // 🔹 Важный выбор: вокруг какой оси вращать?
    // - XMMatrixRotationZ(angle) — если спрайт лежит в плоскости XY (как 2D-картинка в 3D)
    // - XMMatrixRotationY(angle) — если спрайт "стоит" вертикально и должен поворачиваться как объект в мире
    XMMATRIX rotate = XMMatrixRotationZ(m_rotation); // 👈 Поменяйте на RotationY при необходимости

    XMMATRIX translate = XMMatrixTranslation(m_position.x, m_position.y, m_position.z);

    // Умножаем в правильном порядке!
    m_cachedWorldMatrix = scale * rotate * translate;
    m_isMatrixDirty = false;

    return m_cachedWorldMatrix;
}

