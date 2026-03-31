#include "CompartmentDebugRenderer.h"
#include "Components.h"
#include <vector>

CompartmentDebugRenderer::CompartmentDebugRenderer() {}
CompartmentDebugRenderer::~CompartmentDebugRenderer() { Shutdown(); }

// ════════════════════════════════════════════════════════════════════════════
// Initialize — создаём буферы для единичного куба [-0.5, 0.5]³
// ════════════════════════════════════════════════════════════════════════════
bool CompartmentDebugRenderer::Initialize(ID3D11Device* device)
{
    if (!device) return false;
    CreateCubeBuffers(device);
    return (m_vertexBuffer != nullptr && m_indexBuffer != nullptr);
}

void CompartmentDebugRenderer::Shutdown()
{
    if (m_vertexBuffer) { m_vertexBuffer->Release(); m_vertexBuffer = nullptr; }
    if (m_indexBuffer) { m_indexBuffer->Release();  m_indexBuffer = nullptr; }
}

// ════════════════════════════════════════════════════════════════════════════
// CreateCubeBuffers — создаём единичный куб (wireframe edges)
// ════════════════════════════════════════════════════════════════════════════
void CompartmentDebugRenderer::CreateCubeBuffers(ID3D11Device* device)
{
    // 8 вершин куба ([-0.5, 0.5] по всем осям)
    // Цвет будет перезаписываться в шейдере, но задаём белый
    // Цвет будет перезаписываться в шейдере, но задаём белый
    ColorVertex vertices[8] = {
        { XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT4(1, 1, 1, 1) }, // 0
        { XMFLOAT3(0.5f, -0.5f, -0.5f), XMFLOAT4(1, 1, 1, 1) }, // 1
        { XMFLOAT3(0.5f,  0.5f, -0.5f), XMFLOAT4(1, 1, 1, 1) }, // 2
        { XMFLOAT3(-0.5f,  0.5f, -0.5f), XMFLOAT4(1, 1, 1, 1) }, // 3
        { XMFLOAT3(-0.5f, -0.5f,  0.5f), XMFLOAT4(1, 1, 1, 1) }, // 4
        { XMFLOAT3(0.5f, -0.5f,  0.5f), XMFLOAT4(1, 1, 1, 1) }, // 5
        { XMFLOAT3(0.5f,  0.5f,  0.5f), XMFLOAT4(1, 1, 1, 1) }, // 6
        { XMFLOAT3(-0.5f,  0.5f,  0.5f), XMFLOAT4(1, 1, 1, 1) }  // 7
    };

    // Индексы для 12 рёбер куба (wireframe)
    unsigned long indices[24] = {
        // Нижняя грань
        0, 1,  1, 2,  2, 3,  3, 0,
        // Верхняя грань
        4, 5,  5, 6,  6, 7,  7, 4,
        // Вертикальные рёбра
        0, 4,  1, 5,  2, 6,  3, 7
    };

    m_indexCount = 24;

    // ── Vertex Buffer ─────────────────────────────────────────────────────────
    D3D11_BUFFER_DESC vbd = {};
    vbd.Usage = D3D11_USAGE_DEFAULT;
    vbd.ByteWidth = sizeof(vertices);
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vData = {};
    vData.pSysMem = vertices;

    if (FAILED(device->CreateBuffer(&vbd, &vData, &m_vertexBuffer)))
    {
        OutputDebugStringA("[CompartmentDebugRenderer] Failed to create vertex buffer\n");
        return;
    }

    // ── Index Buffer ──────────────────────────────────────────────────────────
    D3D11_BUFFER_DESC ibd = {};
    ibd.Usage = D3D11_USAGE_DEFAULT;
    ibd.ByteWidth = sizeof(indices);
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA iData = {};
    iData.pSysMem = indices;

    if (FAILED(device->CreateBuffer(&ibd, &iData, &m_indexBuffer)))
    {
        OutputDebugStringA("[CompartmentDebugRenderer] Failed to create index buffer\n");
        return;
    }

    OutputDebugStringA("[CompartmentDebugRenderer] Cube buffers created successfully\n");
}

// ════════════════════════════════════════════════════════════════════════════
// RenderCompartments — рендер всех отсеков из ECS
// ════════════════════════════════════════════════════════════════════════════
void CompartmentDebugRenderer::RenderCompartments(
    ECSRegistry& registry,
    ID3D11DeviceContext* context,
    const XMMATRIX& view,
    const XMMATRIX& projection,
    ColorShaderClass* shader)
{
    if (!context || !shader || !m_vertexBuffer || !m_indexBuffer) return;

    UINT stride = sizeof(ColorVertex);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);
    context->IASetIndexBuffer(m_indexBuffer, DXGI_FORMAT_R32_UINT, 0);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

    auto& compPool = registry.GetComponents<CompartmentComponent>();
    auto& transformPool = registry.GetComponents<TransformComponent>();
    auto& subPool = registry.GetComponents<SubmarineComponent>();

    for (auto& [entity, comp] : compPool)
    {
        // 🔥 ИЗМЕНЕНИЕ: Пробуем найти подлодку, но если нет - используем мировые координаты
        auto* subLink = subPool.Get(entity);
        XMMATRIX rootMatrix = XMMatrixIdentity(); // По умолчанию - мировая матрица

        if (subLink && subLink->submarineRoot != INVALID_ENTITY)
        {
            auto* rootTransform = transformPool.Get(subLink->submarineRoot);
            if (rootTransform)
                rootMatrix = rootTransform->GetWorldMatrix();
        }
        else
        {
            // Если нет подлодки - берём трансформ самого объекта
            auto* selfTransform = transformPool.Get(entity);
            if (selfTransform)
                rootMatrix = selfTransform->GetWorldMatrix();
        }

        // Вычисляем центр отсека
        XMFLOAT3 localCenter = {
            (comp.localMin.x + comp.localMax.x) * 0.5f,
            (comp.localMin.y + comp.localMax.y) * 0.5f,
            (comp.localMin.z + comp.localMax.z) * 0.5f
        };

        // Преобразуем в мировые координаты
        XMVECTOR localCenterV = XMLoadFloat3(&localCenter);
        XMVECTOR worldCenterV = XMVector3TransformCoord(localCenterV, rootMatrix);
        XMFLOAT3 worldCenter;
        XMStoreFloat3(&worldCenter, worldCenterV);

        // Размеры
        float sizeX = comp.localMax.x - comp.localMin.x;
        float sizeY = comp.localMax.y - comp.localMin.y;
        float sizeZ = comp.localMax.z - comp.localMin.z;

        // 🔥 ВАЖНО: Если размеры нулевые - пропускаем
        if (sizeX <= 0.001f || sizeY <= 0.001f || sizeZ <= 0.001f)
        {
            OutputDebugStringA("[CompartmentDebug] SKIP: Zero size bounds!\n");
            continue;
        }

        // Матрица трансформации
        XMMATRIX world =
            XMMatrixScaling(sizeX, sizeY, sizeZ) *
            XMMatrixTranslation(worldCenter.x, worldCenter.y, worldCenter.z);

        // Цвет (зелёный по умолчанию для отладки)
        XMFLOAT4 color = XMFLOAT4(0.0f, 1.0f, 0.3f, 0.6f);

        float floodPercent = (comp.volume > 0.0f) ? (comp.waterVolume / comp.volume) : 0.0f;


        if (floodPercent < 0.01f)
            color = XMFLOAT4(0.0f, 1.0f, 0.3f, 0.6f);  // Зелёный
        else if (floodPercent < 0.5f)
            color = XMFLOAT4(1.0f, 1.0f, 0.0f, 0.7f);  // Жёлтый
        else if (floodPercent < 0.9f)
            color = XMFLOAT4(1.0f, 0.5f, 0.0f, 0.8f);  // Оранжевый
        else
            color = XMFLOAT4(1.0f, 0.0f, 0.0f, 0.9f);  // Красный

        // 🔥 DEBUG вывод
        char buf[256];
        sprintf_s(buf,
            "[CompartmentDebug] Entity %u | Pos=(%.2f,%.2f,%.2f) | Size=(%.2f,%.2f,%.2f)\n",
            entity, worldCenter.x, worldCenter.y, worldCenter.z,
            sizeX, sizeY, sizeZ);
        OutputDebugStringA(buf);

        // Рендер
        shader->Render(context, m_indexCount, world, view, projection, color);
    }
}