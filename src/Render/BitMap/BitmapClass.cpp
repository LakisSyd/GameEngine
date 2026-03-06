#include "BitmapClass.h"

BitmapClass::BitmapClass()
{
	m_vertexBuffer = nullptr;
	m_indexBuffer = nullptr;
	m_Texture = nullptr;

}

BitmapClass::BitmapClass(const BitmapClass&)
{
}

BitmapClass::~BitmapClass()
{
}


bool BitmapClass::Initialize(ID3D11Device* device, ID3D11DeviceContext* deviceContext,
	int screenWidth, int screenHeight, const char* textureFilename,
	int renderX, int renderY)
{
	bool result;

	m_screenHeight = screenHeight;
	m_screenWidth = screenWidth;

	m_renderX = renderX;
	m_renderY = renderY;


	result = InitializeBuffers(device);
	if (!result)
	{
		return false;
	}

	result = LoadTexture(device, deviceContext, textureFilename);
	if (!result)
	{
		return false;
	}

	return true;


}
bool BitmapClass::Initialize(ID3D11Device* device, ID3D11DeviceContext* deviceContext,
    int screenWidth, int screenHeight, int bitmapWidth, int bitmapHeight)
{
    m_screenWidth = screenWidth;
    m_screenHeight = screenHeight;
    m_bitmapWidth = bitmapWidth;
    m_bitmapHeight = bitmapHeight;
    m_renderX = 0;
    m_renderY = 0;
    m_Texture = nullptr; // текстура не нужна — будем передавать SRV снаружи

    return InitializeBuffers(device);
}

void BitmapClass::Shutdown()
{
	ReleaseTexture();
	ShutdownBuffers();
	return;
}

bool BitmapClass::Render(ID3D11DeviceContext* deviceContext)
{
	bool result;
	result = UpdateBuffers(deviceContext);
	if (!result)
	{
		return false;
	}

	RenderBuffers(deviceContext);

	return true;
}

int BitmapClass::GetIndexCount()
{
	// Всегда возвращает 6 (два треугольника)
	return m_indexCount;
}

ID3D11ShaderResourceView* BitmapClass::GetTexture()
{
	return m_Texture->GetTexture();
}




bool BitmapClass::InitializeBuffers(ID3D11Device* device)
{
	VertexType* vertices;
	unsigned long* indices;

	D3D11_BUFFER_DESC vertexBufferDesc, indexBufferDesc;
	D3D11_SUBRESOURCE_DATA vertexData, indexData;
	HRESULT result;
	int i;

    // Инициализируем предыдущую позицию отрисовки значением -1
    m_prevPosX = -1;
    m_prevPosY = -1;

    // Устанавливаем количество вершин (квад из двух треугольников = 6 точек)
    m_vertexCount = 6;
    m_indexCount = m_vertexCount;

    // Создаем массив вершин
    vertices = new VertexType[m_vertexCount];
    if (!vertices)
    {
        return false;
    }

    // Создаем массив индексов
    indices = new unsigned long[m_indexCount];
    if (!indices)
    {
        delete[] vertices;
        return false;
    }

    // Инициализируем массив вершин нулями
    memset(vertices, 0, (sizeof(VertexType) * m_vertexCount));

    // Загружаем массив индексов данными
    for (i = 0; i < m_indexCount; i++)
    {
        indices[i] = i;
    }

    // Настраиваем описание динамического буфера вершин
    // D3D11_USAGE_DYNAMIC позволяет изменять данные каждый кадр
    vertexBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    vertexBufferDesc.ByteWidth = sizeof(VertexType) * m_vertexCount;
    vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vertexBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    vertexBufferDesc.MiscFlags = 0;
    vertexBufferDesc.StructureByteStride = 0;

    // Указываем pointer на данные вершин
    vertexData.pSysMem = vertices;
    vertexData.SysMemPitch = 0;
    vertexData.SysMemSlicePitch = 0;

    // Создаем буфер вершин
    result = device->CreateBuffer(&vertexBufferDesc, &vertexData, &m_vertexBuffer);
    if (FAILED(result))
    {
        delete[] vertices;
        delete[] indices;
        return false;
    }

    // Буфер индексов не нужно делать динамическим, индексы всегда одинаковы
    indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    indexBufferDesc.ByteWidth = sizeof(unsigned long) * m_indexCount;
    indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    indexBufferDesc.CPUAccessFlags = 0;
    indexBufferDesc.MiscFlags = 0;
    indexBufferDesc.StructureByteStride = 0;

    indexData.pSysMem = indices;
    indexData.SysMemPitch = 0;
    indexData.SysMemSlicePitch = 0;

    // Создаем буфер индексов
    result = device->CreateBuffer(&indexBufferDesc, &indexData, &m_indexBuffer);
    if (FAILED(result))
    {
        delete[] vertices;
        delete[] indices;
        return false;
    }

    // Освобождаем временные массивы
    delete[] vertices;
    vertices = 0;
    delete[] indices;
    indices = 0;

    return true;
}

void BitmapClass::ShutdownBuffers()
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

bool BitmapClass::UpdateBuffers(ID3D11DeviceContext* deviceContext)
{
    if ((m_prevPosX == m_renderX) && (m_prevPosY == m_renderY))
        return true;
    m_prevPosX = m_renderX;
    m_prevPosY = m_renderY;

    // Используем экранные координаты: левый верхний угол = (m_renderX, m_renderY)
    float left = (float)m_renderX;
    float right = left + (float)m_bitmapWidth;
    float top = (float)m_renderY;
    float bottom = top + (float)m_bitmapHeight;  // Y увеличивается вниз (как в ортографической матрице)

    char dbg[256];
    sprintf_s(dbg, "UpdateBuffers: renderX=%d renderY=%d\n", m_renderX, m_renderY);

    OutputDebugStringA(dbg);
    sprintf_s(dbg, "Coords: left=%f right=%f top=%f bottom=%f\n", left, right, top, bottom);
    OutputDebugStringA(dbg);

    // Z должно быть больше screenNear (например, 0.1f), чтобы объект не отсекался
    const float z = 0.5f;

    VertexType vertices[6] =
    {
        // Первый треугольник
        { { left,  top,    z }, { 0.0f, 0.0f } },
        { { right, top,    z }, { 1.0f, 0.0f } },
        { { left,  bottom, z }, { 0.0f, 1.0f } },
        // Второй треугольник
        { { right, top,    z }, { 1.0f, 0.0f } },
        { { right, bottom, z }, { 1.0f, 1.0f } },
        { { left,  bottom, z }, { 0.0f, 1.0f } }
    };

    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = deviceContext->Map(m_vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr)) return false;

    memcpy(mapped.pData, vertices, sizeof(vertices));
    deviceContext->Unmap(m_vertexBuffer, 0);

    return true;
}
bool BitmapClass::LoadTexture(ID3D11Device* device, ID3D11DeviceContext* deviceContext, const char* filename)
{
    bool result;

    // Создаем объект текстуры
    m_Texture = new TextureClass;
    result = m_Texture->Initialize(device, deviceContext, filename);
    if (!result)
    {
        return false;
    }

    // Сохраняем размеры текстуры в пикселях
    m_bitmapWidth = m_Texture->GetWidth();
    m_bitmapHeight = m_Texture->GetHeight();

    return true;
}

void BitmapClass::ReleaseTexture()
{
    if (m_Texture)
    {
        m_Texture->Shutdown();
        delete m_Texture;
        m_Texture = 0;
    }
    return;
}

void BitmapClass::SetRenderLocation(int x, int y)
{
    m_renderX = x;
    m_renderY = y;
    return;
}

void BitmapClass::RenderBuffers(ID3D11DeviceContext* deviceContext)
{
    unsigned int stride;
    unsigned int offset;

    // Устанавливаем шаг и смещение буфера вершин
    stride = sizeof(VertexType);
    offset = 0;

    // Активируем буфер вершин
    deviceContext->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);

    // Активируем буфер индексов
    deviceContext->IASetIndexBuffer(m_indexBuffer, DXGI_FORMAT_R32_UINT, 0);

    // Устанавливаем топологию примитивов (список треугольников)
    deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    return;
}