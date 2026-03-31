#include "PhysicsDebugRenderer.h"
#include "Components.h"
#include <vector>
#include <bullet/BulletCollision/CollisionDispatch/btCollisionWorld.h>
#include <bullet/BulletCollision/CollisionDispatch/btCollisionDispatcher.h>
#include <bullet/BulletCollision/NarrowPhaseCollision/btPersistentManifold.h>

PhysicsDebugRenderer::PhysicsDebugRenderer() {}
PhysicsDebugRenderer::~PhysicsDebugRenderer() { Shutdown(); }

// ════════════════════════════════════════════════════════════════════════════
// Initialize
// ════════════════════════════════════════════════════════════════════════════
bool PhysicsDebugRenderer::Initialize(ID3D11Device* device)
{
    if (!device) return false;

    CreateCube(device);
    CreateSphere(device, 16);
    CreateCapsule(device, 12);
    CreateCylinder(device, 16);
    CreateLine(device);

    OutputDebugStringA("[PhysicsDebugRenderer] Initialized successfully\n");
    return true;
}

void PhysicsDebugRenderer::Shutdown()
{
    auto ReleasePrimitive = [](PrimitiveBuffers& prim) {
        if (prim.vertexBuffer) { prim.vertexBuffer->Release(); prim.vertexBuffer = nullptr; }
        if (prim.indexBuffer) { prim.indexBuffer->Release(); prim.indexBuffer = nullptr; }
        };

    ReleasePrimitive(m_cubeBuffers);
    ReleasePrimitive(m_sphereBuffers);
    ReleasePrimitive(m_capsuleBuffers);
    ReleasePrimitive(m_cylinderBuffers);
    ReleasePrimitive(m_lineBuffers);
}

// ════════════════════════════════════════════════════════════════════════════
// CreateCube - 12 рёбер wireframe куба
// ════════════════════════════════════════════════════════════════════════════
void PhysicsDebugRenderer::CreateCube(ID3D11Device* device)
{
    ColorVertex vertices[8] = {
        { XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT4(1, 1, 1, 1) },
        { XMFLOAT3(0.5f, -0.5f, -0.5f), XMFLOAT4(1, 1, 1, 1) },
        { XMFLOAT3(0.5f,  0.5f, -0.5f), XMFLOAT4(1, 1, 1, 1) },
        { XMFLOAT3(-0.5f,  0.5f, -0.5f), XMFLOAT4(1, 1, 1, 1) },
        { XMFLOAT3(-0.5f, -0.5f,  0.5f), XMFLOAT4(1, 1, 1, 1) },
        { XMFLOAT3(0.5f, -0.5f,  0.5f), XMFLOAT4(1, 1, 1, 1) },
        { XMFLOAT3(0.5f,  0.5f,  0.5f), XMFLOAT4(1, 1, 1, 1) },
        { XMFLOAT3(-0.5f,  0.5f,  0.5f), XMFLOAT4(1, 1, 1, 1) }
    };

    unsigned long indices[24] = {
        0,1, 1,2, 2,3, 3,0,  // Нижняя грань
        4,5, 5,6, 6,7, 7,4,  // Верхняя грань
        0,4, 1,5, 2,6, 3,7   // Вертикальные рёбра
    };

    m_cubeBuffers.indexCount = 24;

    D3D11_BUFFER_DESC vbd = {};
    vbd.Usage = D3D11_USAGE_DEFAULT;
    vbd.ByteWidth = sizeof(vertices);
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vData = { vertices };
    device->CreateBuffer(&vbd, &vData, &m_cubeBuffers.vertexBuffer);

    D3D11_BUFFER_DESC ibd = {};
    ibd.Usage = D3D11_USAGE_DEFAULT;
    ibd.ByteWidth = sizeof(indices);
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA iData = { indices };
    device->CreateBuffer(&ibd, &iData, &m_cubeBuffers.indexBuffer);
}

// ════════════════════════════════════════════════════════════════════════════
// CreateSphere - wireframe сфера из параллелей и меридианов
// ════════════════════════════════════════════════════════════════════════════
void PhysicsDebugRenderer::CreateSphere(ID3D11Device* device, int segments)
{
    std::vector<ColorVertex> vertices;
    std::vector<unsigned long> indices;

    const float radius = 0.5f;
    const int rings = segments / 2;
    const int slices = segments;

    // Генерация вершин
    for (int ring = 0; ring <= rings; ++ring)
    {
        float phi = XM_PI * float(ring) / float(rings);
        for (int slice = 0; slice <= slices; ++slice)
        {
            float theta = XM_2PI * float(slice) / float(slices);

            XMFLOAT3 pos;
            pos.x = radius * sinf(phi) * cosf(theta);
            pos.y = radius * cosf(phi);
            pos.z = radius * sinf(phi) * sinf(theta);

            vertices.push_back({ pos, XMFLOAT4(1, 1, 1, 1) });
        }
    }

    // Генерация индексов (линии между вершинами)
    for (int ring = 0; ring < rings; ++ring)
    {
        for (int slice = 0; slice < slices; ++slice)
        {
            int current = ring * (slices + 1) + slice;
            int next = current + slices + 1;

            // Горизонтальные линии
            indices.push_back(current);
            indices.push_back(current + 1);

            // Вертикальные линии
            indices.push_back(current);
            indices.push_back(next);
        }
    }

    m_sphereBuffers.indexCount = (int)indices.size();

    D3D11_BUFFER_DESC vbd = {};
    vbd.Usage = D3D11_USAGE_DEFAULT;
    vbd.ByteWidth = (UINT)(vertices.size() * sizeof(ColorVertex));
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vData = { vertices.data() };
    device->CreateBuffer(&vbd, &vData, &m_sphereBuffers.vertexBuffer);

    D3D11_BUFFER_DESC ibd = {};
    ibd.Usage = D3D11_USAGE_DEFAULT;
    ibd.ByteWidth = (UINT)(indices.size() * sizeof(unsigned long));
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA iData = { indices.data() };
    device->CreateBuffer(&ibd, &iData, &m_sphereBuffers.indexBuffer);
}

// ════════════════════════════════════════════════════════════════════════════
// CreateCapsule - капсула (цилиндр + 2 полусферы)
// ════════════════════════════════════════════════════════════════════════════
void PhysicsDebugRenderer::CreateCapsule(ID3D11Device* device, int segments)
{
    std::vector<ColorVertex> vertices;
    std::vector<unsigned long> indices;

    const float radius = 0.5f;
    const float halfHeight = 0.5f;

    // Верхняя полусфера
    for (int i = 0; i <= segments / 2; ++i)
    {
        float phi = XM_PIDIV2 * float(i) / float(segments / 2);
        for (int j = 0; j <= segments; ++j)
        {
            float theta = XM_2PI * float(j) / float(segments);
            XMFLOAT3 pos;
            pos.x = radius * sinf(phi) * cosf(theta);
            pos.y = halfHeight + radius * cosf(phi);
            pos.z = radius * sinf(phi) * sinf(theta);
            vertices.push_back({ pos, XMFLOAT4(1, 1, 1, 1) });
        }
    }

    // Цилиндр (вертикальные линии)
    for (int j = 0; j <= segments; ++j)
    {
        float theta = XM_2PI * float(j) / float(segments);
        XMFLOAT3 top = { radius * cosf(theta), halfHeight, radius * sinf(theta) };
        XMFLOAT3 bottom = { radius * cosf(theta), -halfHeight, radius * sinf(theta) };

        int baseIdx = (int)vertices.size();
        vertices.push_back({ top, XMFLOAT4(1, 1, 1, 1) });
        vertices.push_back({ bottom, XMFLOAT4(1, 1, 1, 1) });

        indices.push_back(baseIdx);
        indices.push_back(baseIdx + 1);
    }

    // Индексы для полусферы (аналогично сфере)
    int ringsInHalf = segments / 2;
    for (int ring = 0; ring < ringsInHalf; ++ring)
    {
        for (int slice = 0; slice < segments; ++slice)
        {
            int current = ring * (segments + 1) + slice;
            int next = current + segments + 1;

            indices.push_back(current);
            indices.push_back(current + 1);
            indices.push_back(current);
            indices.push_back(next);
        }
    }

    m_capsuleBuffers.indexCount = (int)indices.size();

    D3D11_BUFFER_DESC vbd = {};
    vbd.Usage = D3D11_USAGE_DEFAULT;
    vbd.ByteWidth = (UINT)(vertices.size() * sizeof(ColorVertex));
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vData = { vertices.data() };
    device->CreateBuffer(&vbd, &vData, &m_capsuleBuffers.vertexBuffer);

    D3D11_BUFFER_DESC ibd = {};
    ibd.Usage = D3D11_USAGE_DEFAULT;
    ibd.ByteWidth = (UINT)(indices.size() * sizeof(unsigned long));
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA iData = { indices.data() };
    device->CreateBuffer(&ibd, &iData, &m_capsuleBuffers.indexBuffer);
}

// ════════════════════════════════════════════════════════════════════════════
// CreateCylinder
// ════════════════════════════════════════════════════════════════════════════
void PhysicsDebugRenderer::CreateCylinder(ID3D11Device* device, int segments)
{
    std::vector<ColorVertex> vertices;
    std::vector<unsigned long> indices;

    const float radius = 0.5f;
    const float halfHeight = 0.5f;

    // Верхнее кольцо
    for (int i = 0; i <= segments; ++i)
    {
        float theta = XM_2PI * float(i) / float(segments);
        XMFLOAT3 pos = { radius * cosf(theta), halfHeight, radius * sinf(theta) };
        vertices.push_back({ pos, XMFLOAT4(1, 1, 1, 1) });
    }

    // Нижнее кольцо
    for (int i = 0; i <= segments; ++i)
    {
        float theta = XM_2PI * float(i) / float(segments);
        XMFLOAT3 pos = { radius * cosf(theta), -halfHeight, radius * sinf(theta) };
        vertices.push_back({ pos, XMFLOAT4(1, 1, 1, 1) });
    }

    // Индексы для колец и вертикальных линий
    for (int i = 0; i < segments; ++i)
    {
        // Верхнее кольцо
        indices.push_back(i);
        indices.push_back(i + 1);

        // Нижнее кольцо
        indices.push_back(segments + 1 + i);
        indices.push_back(segments + 1 + i + 1);

        // Вертикальные линии
        indices.push_back(i);
        indices.push_back(segments + 1 + i);
    }

    m_cylinderBuffers.indexCount = (int)indices.size();

    D3D11_BUFFER_DESC vbd = {};
    vbd.Usage = D3D11_USAGE_DEFAULT;
    vbd.ByteWidth = (UINT)(vertices.size() * sizeof(ColorVertex));
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vData = { vertices.data() };
    device->CreateBuffer(&vbd, &vData, &m_sphereBuffers.vertexBuffer);

    D3D11_BUFFER_DESC ibd = {};
    ibd.Usage = D3D11_USAGE_DEFAULT;
    ibd.ByteWidth = (UINT)(indices.size() * sizeof(unsigned long));
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA iData = { indices.data() };
    device->CreateBuffer(&ibd, &iData, &m_cylinderBuffers.indexBuffer);
}

// ════════════════════════════════════════════════════════════════════════════
// CreateLine - простая линия для векторов
// ════════════════════════════════════════════════════════════════════════════
void PhysicsDebugRenderer::CreateLine(ID3D11Device* device)
{
    ColorVertex vertices[2] = {
        { XMFLOAT3(0, 0, 0), XMFLOAT4(1, 1, 1, 1) },
        { XMFLOAT3(0, 1, 0), XMFLOAT4(1, 1, 1, 1) }
    };

    unsigned long indices[2] = { 0, 1 };
    m_lineBuffers.indexCount = 2;

    D3D11_BUFFER_DESC vbd = {};
    vbd.Usage = D3D11_USAGE_DEFAULT;
    vbd.ByteWidth = sizeof(vertices);
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vData = { vertices };
    device->CreateBuffer(&vbd, &vData, &m_lineBuffers.vertexBuffer);

    D3D11_BUFFER_DESC ibd = {};
    ibd.Usage = D3D11_USAGE_DEFAULT;
    ibd.ByteWidth = sizeof(indices);
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA iData = { indices };
    device->CreateBuffer(&ibd, &iData, &m_lineBuffers.indexBuffer);
}

// ════════════════════════════════════════════════════════════════════════════
// Render - главный метод
// ════════════════════════════════════════════════════════════════════════════
void PhysicsDebugRenderer::Render(
    ECSRegistry& registry,
    btCollisionWorld* world,
    ID3D11DeviceContext* context,
    const XMMATRIX& view,
    const XMMATRIX& projection,
    ColorShaderClass* shader,
    const PhysicsDebugSettings& settings)
{
    if (!context || !shader) return;

    m_settings = settings;

    // ═══════════════════════════════════════════════════════════════════
    // 1. РЕНДЕР КОЛЛАЙДЕРОВ
    // ═══════════════════════════════════════════════════════════════════
    if (settings.showColliders)
    {
        auto& rigidPool = registry.GetComponents<RigidBodyComponent>();

        for (auto& [entity, rb] : rigidPool)
        {
            if (!registry.HasComponent<ActiveComponent>(entity)) continue;
            if (!rb.body) continue;

            btCollisionShape* shape = rb.body->getCollisionShape();
            if (!shape) continue;

            btTransform transform = rb.body->getWorldTransform();
            XMFLOAT4 color = GetColorForRigidBody(rb.body);

            // Проверяем фильтры
            switch (shape->getShapeType())
            {
            case BOX_SHAPE_PROXYTYPE:
                if (!settings.showBoxes) continue;
                {
                    btBoxShape* box = static_cast<btBoxShape*>(shape);
                    RenderBox(context, box->getHalfExtentsWithMargin(), transform, view, projection, shader, color);
                }
                break;

            case SPHERE_SHAPE_PROXYTYPE:
                if (!settings.showSpheres) continue;
                {
                    btSphereShape* sphere = static_cast<btSphereShape*>(shape);
                    RenderSphere(context, sphere->getRadius(), transform.getOrigin(), view, projection, shader, color);
                }
                break;

            case CAPSULE_SHAPE_PROXYTYPE:
                if (!settings.showCapsules) continue;
                {
                    btCapsuleShape* capsule = static_cast<btCapsuleShape*>(shape);
                    RenderCapsule(context, capsule->getRadius(), capsule->getHalfHeight() * 2.0f, transform, view, projection, shader, color);
                }
                break;

            case CYLINDER_SHAPE_PROXYTYPE:
                if (!settings.showCylinders) continue;
                {
                    btCylinderShape* cyl = static_cast<btCylinderShape*>(shape);
                    btVector3 extents = cyl->getHalfExtentsWithMargin();
                    RenderBox(context, extents, transform, view, projection, shader, color); // Упрощение
                }
                break;

            case STATIC_PLANE_PROXYTYPE:
                if (!settings.showPlanes) continue;
                {
                    btVector3 halfExtents(10.0f, 0.01f, 10.0f);
                    RenderBox(context, halfExtents, transform, view, projection, shader, settings.colorStatic);
                }
                break;

            case CONVEX_HULL_SHAPE_PROXYTYPE:
                if (!settings.showConvexHulls && settings.showAABB)
                {
                    btVector3 aabbMin, aabbMax;
                    rb.body->getAabb(aabbMin, aabbMax);
                    RenderAABB(context, aabbMin, aabbMax, view, projection, shader);
                }
                break;

            case TRIANGLE_MESH_SHAPE_PROXYTYPE:
                if (!settings.showTriangleMeshes && settings.showAABB)
                {
                    btVector3 aabbMin, aabbMax;
                    rb.body->getAabb(aabbMin, aabbMax);
                    RenderAABB(context, aabbMin, aabbMax, view, projection, shader);
                }
                break;
            }

            // Рендер AABB для ВСЕХ объектов (если включено)
            if (settings.showAABB)
            {
                btVector3 aabbMin, aabbMax;
                rb.body->getAabb(aabbMin, aabbMax);
                RenderAABB(context, aabbMin, aabbMax, view, projection, shader);
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════
    // 2. РЕНДЕР КОНТАКТОВ
    // ═══════════════════════════════════════════════════════════════════
    if (settings.showContacts && world)
    {
        btDispatcher* dispatcher = world->getDispatcher();
        if (dispatcher)
        {
            int numManifolds = dispatcher->getNumManifolds();
            for (int i = 0; i < numManifolds; ++i)
            {
                btPersistentManifold* manifold = dispatcher->getManifoldByIndexInternal(i);
                if (!manifold) continue;

                for (int p = 0; p < manifold->getNumContacts(); ++p)
                {
                    const btManifoldPoint& pt = manifold->getContactPoint(p);
                    btVector3 posB = pt.getPositionWorldOnB();
                    btVector3 normal = pt.m_normalWorldOnB;

                    XMFLOAT3 position(posB.x(), posB.y(), posB.z());
                    XMFLOAT3 normalDir(normal.x(), normal.y(), normal.z());

                    RenderContactPoint(context, position, normalDir, view, projection, shader);
                }
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════
    // 3. РЕНДЕР ЦЕНТРОВ МАСС
    // ═══════════════════════════════════════════════════════════════════
    if (settings.showCenterOfMass)
    {
        auto& rigidPool = registry.GetComponents<RigidBodyComponent>();
        for (auto& [entity, rb] : rigidPool)
        {
            if (!rb.body || rb.mass <= 0.0f) continue;

            btVector3 pos = rb.body->getCenterOfMassPosition();
            RenderSphere(context, 0.15f, pos, view, projection, shader, XMFLOAT4(1, 0, 1, 1)); // Фиолетовый
        }
    }

    // ═══════════════════════════════════════════════════════════════════
    // 4. РЕНДЕР ВЕКТОРОВ СКОРОСТЕЙ
    // ═══════════════════════════════════════════════════════════════════
    if (settings.showVelocity)
    {
        // TODO: Нарисовать стрелки от центра масс в направлении скорости
    }
}

// ════════════════════════════════════════════════════════════════════════════
// RenderBox
// ════════════════════════════════════════════════════════════════════════════
void PhysicsDebugRenderer::RenderBox(
    ID3D11DeviceContext* context,
    const btVector3& halfExtents,
    const btTransform& btTransform,
    const XMMATRIX& view,
    const XMMATRIX& projection,
    ColorShaderClass* shader,
    const XMFLOAT4& color)
{
    btVector3 pos = btTransform.getOrigin();
    btQuaternion rot = btTransform.getRotation();

    XMMATRIX world =
        XMMatrixScaling(halfExtents.x() * 2.0f, halfExtents.y() * 2.0f, halfExtents.z() * 2.0f) *
        XMMatrixRotationQuaternion(XMVectorSet(rot.x(), rot.y(), rot.z(), rot.w())) *
        XMMatrixTranslation(pos.x(), pos.y(), pos.z());

    RenderCollider(context, world, view, projection, shader, m_cubeBuffers, color);
}

// ════════════════════════════════════════════════════════════════════════════
// RenderSphere
// ════════════════════════════════════════════════════════════════════════════
void PhysicsDebugRenderer::RenderSphere(
    ID3D11DeviceContext* context,
    float radius,
    const btVector3& position,
    const XMMATRIX& view,
    const XMMATRIX& projection,
    ColorShaderClass* shader,
    const XMFLOAT4& color)
{
    XMMATRIX world =
        XMMatrixScaling(radius * 2.0f, radius * 2.0f, radius * 2.0f) *
        XMMatrixTranslation(position.x(), position.y(), position.z());

    RenderCollider(context, world, view, projection, shader, m_sphereBuffers, color);
}

// ════════════════════════════════════════════════════════════════════════════
// RenderCapsule
// ════════════════════════════════════════════════════════════════════════════
void PhysicsDebugRenderer::RenderCapsule(
    ID3D11DeviceContext* context,
    float radius,
    float height,
    const btTransform& btTransform,
    const XMMATRIX& view,
    const XMMATRIX& projection,
    ColorShaderClass* shader,
    const XMFLOAT4& color)
{
    btVector3 pos = btTransform.getOrigin();
    btQuaternion rot = btTransform.getRotation();

    XMMATRIX world =
        XMMatrixScaling(radius * 2.0f, height, radius * 2.0f) *
        XMMatrixRotationQuaternion(XMVectorSet(rot.x(), rot.y(), rot.z(), rot.w())) *
        XMMatrixTranslation(pos.x(), pos.y(), pos.z());

    RenderCollider(context, world, view, projection, shader, m_capsuleBuffers, color);
}

// ════════════════════════════════════════════════════════════════════════════
// RenderAABB - axis-aligned bounding box (для ConvexHull/TriangleMesh)
// ════════════════════════════════════════════════════════════════════════════
void PhysicsDebugRenderer::RenderAABB(
    ID3D11DeviceContext* context,
    const btVector3& min,
    const btVector3& max,
    const XMMATRIX& view,
    const XMMATRIX& projection,
    ColorShaderClass* shader)
{
    btVector3 center = (min + max) * 0.5f;
    btVector3 extents = (max - min) * 0.5f;

    XMMATRIX world =
        XMMatrixScaling(extents.x() * 2.0f, extents.y() * 2.0f, extents.z() * 2.0f) *
        XMMatrixTranslation(center.x(), center.y(), center.z());

    RenderCollider(context, world, view, projection, shader, m_cubeBuffers, m_settings.colorAABB);
}

// ════════════════════════════════════════════════════════════════════════════
// RenderContactPoint - маркер + нормаль
// ════════════════════════════════════════════════════════════════════════════
void PhysicsDebugRenderer::RenderContactPoint(
    ID3D11DeviceContext* context,
    const XMFLOAT3& position,
    const XMFLOAT3& normal,
    const XMMATRIX& view,
    const XMMATRIX& projection,
    ColorShaderClass* shader)
{
    // Маркер (маленькая сфера)
    RenderSphere(context, m_settings.contactMarkerSize,
        btVector3(position.x, position.y, position.z),
        view, projection, shader, m_settings.colorContact);

    // TODO: Нарисовать линию нормали
}

// ════════════════════════════════════════════════════════════════════════════
// RenderCollider - универсальный рендер примитива
// ════════════════════════════════════════════════════════════════════════════
void PhysicsDebugRenderer::RenderCollider(
    ID3D11DeviceContext* context,
    const XMMATRIX& world,
    const XMMATRIX& view,
    const XMMATRIX& projection,
    ColorShaderClass* shader,
    const PrimitiveBuffers& buffers,
    const XMFLOAT4& color)
{
    if (!buffers.vertexBuffer || !buffers.indexBuffer) return;

    UINT stride = sizeof(ColorVertex);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, &buffers.vertexBuffer, &stride, &offset);
    context->IASetIndexBuffer(buffers.indexBuffer, DXGI_FORMAT_R32_UINT, 0);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

    shader->Render(context, buffers.indexCount, world, view, projection);
}

// ════════════════════════════════════════════════════════════════════════════
// GetColorForRigidBody - автоматический выбор цвета
// ════════════════════════════════════════════════════════════════════════════
XMFLOAT4 PhysicsDebugRenderer::GetColorForRigidBody(const btRigidBody* body)
{
    if (!body) return m_settings.colorStatic;

    // Проверяем активность
    if (!body->isActive())
        return m_settings.colorSleeping;

    // Кинематические объекты
    if (body->isKinematicObject())
        return m_settings.colorKinematic;

    // Статические объекты
    if (body->isStaticObject())
        return m_settings.colorStatic;

    // Динамические объекты
    return m_settings.colorDynamic;
}
