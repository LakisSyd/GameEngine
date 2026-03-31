
#include "DebugPhysicsVisualizer.h"
#include <windows.h>
#include <cstdio>
#include <cinttypes>

#include "GameObject.h"
#include <bullet/BulletCollision/CollisionDispatch/btCollisionDispatcher.h>
#include <bullet/BulletCollision/NarrowPhaseCollision/btPersistentManifold.h>
#include <bullet/BulletCollision/CollisionDispatch/btCollisionWorld.h>
#include <bullet/LinearMath/btVector3.h>
#include "ECSRegistry .h"
#include "Components.h"

DebugPhysicsVisualizer::DebugPhysicsVisualizer() {}
DebugPhysicsVisualizer::~DebugPhysicsVisualizer() {}

void DebugPhysicsVisualizer::UpdateFromWorld(btCollisionWorld* world)
{
    m_contacts.clear();

    if (!world)
    {
        OutputDebugStringA("DebugPhysicsVisualizer::UpdateFromWorld: world == nullptr\n");
        return;
    }

    btDispatcher* dispatcher = const_cast<btDispatcher*>(world->getDispatcher());
    if (!dispatcher)
    {
        OutputDebugStringA("DebugPhysicsVisualizer::UpdateFromWorld: dispatcher == nullptr\n");
        return;
    }

    int numManifolds = dispatcher->getNumManifolds();
    char header[256];
  
    OutputDebugStringA(header);

    for (int i = 0; i < numManifolds; ++i)
    {
        btPersistentManifold* manifold = dispatcher->getManifoldByIndexInternal(i);
        if (!manifold) continue;

        const btCollisionObject* obA = manifold->getBody0();
        const btCollisionObject* obB = manifold->getBody1();

        int contacts = manifold->getNumContacts();
        if (contacts <= 0) continue;

        for (int p = 0; p < contacts; ++p)
        {
            const btManifoldPoint& pt = manifold->getContactPoint(p);
            btVector3 posB = pt.getPositionWorldOnB();
            btVector3 normal = pt.m_normalWorldOnB;

            DebugContact dc;
            dc.position = XMFLOAT3(posB.getX(), posB.getY(), posB.getZ());
            dc.normal = XMFLOAT3(normal.getX(), normal.getY(), normal.getZ());
            dc.bodyA = obA;
            dc.bodyB = obB;
            dc.distance = pt.getDistance();

            m_contacts.push_back(dc);

            char buf[256];
            
        }
    }
}

void DebugPhysicsVisualizer::RenderContacts(
    ID3D11DeviceContext* context,
    XMMATRIX view,
    XMMATRIX projection,
    AssimpModelClass* sphereModel,
    TextureShaderClass* textureShader,
    float scale)
{
    if (!context || !sphereModel || !textureShader) return;

    for (const auto& c : m_contacts)
    {
        GameObject marker;
        marker.Initialize(sphereModel, c.position.x, c.position.y, c.position.z);
        marker.SetScale(scale, scale, scale);
        marker.RenderSimple(context, view, projection, textureShader);
    }
}




// === RenderContacts для ColorShaderClass ===
void DebugPhysicsVisualizer::RenderContacts(
    ID3D11DeviceContext* context,
    XMMATRIX view,
    XMMATRIX projection,
    AssimpModelClass* sphereModel,
    ColorShaderClass* colorShader,
    float scale)
{
    if (!context || !sphereModel || !colorShader) return;

    for (const auto& c : m_contacts)
    {
        GameObject marker;
        marker.Initialize(sphereModel, c.position.x, c.position.y, c.position.z);
        marker.SetScale(scale, scale, scale);
        marker.RenderSimpleColor(context, view, projection, colorShader); // ← НОВЫЙ МЕТОД
    }
}

void DebugPhysicsVisualizer::RenderCollisionShapes(
    ECSRegistry& registry,
    ID3D11DeviceContext* context,
    XMMATRIX view,
    XMMATRIX projection,
    AssimpModelClass* sphereModel,
    AssimpModelClass* boxModel,
    AssimpModelClass* capsuleModel,
    TextureShaderClass* textureShader,
    float alpha)
{
    if (!context || !textureShader) return;

    auto& rigidPool = registry.GetComponents<RigidBodyComponent>();
    auto& transformPool = registry.GetComponents<TransformComponent>();

    for (auto& [entity, rb] : rigidPool)
    {
        if (!registry.HasComponent<ActiveComponent>(entity)) continue;
        auto* transform = transformPool.Get(entity);
        if (!transform || !rb.body) continue;

        // 🔹 Позиция из Bullet
        btVector3 bulletPos = rb.body->getWorldTransform().getOrigin();
        XMFLOAT3 pos(bulletPos.x(), bulletPos.y(), bulletPos.z());
        XMFLOAT3 rot = transform->rotation;

        AssimpModelClass* model = nullptr;
        XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f }; // Дефолт

        // 🔹 Вычисляем масштаб на основе формы из Bullet
        switch (rb.shapeType)
        {
        case RigidBodyComponent::Box:
        {
            model = boxModel;
            if (rb.body->getCollisionShape()->getShapeType() == BOX_SHAPE_PROXYTYPE)
            {
                btBoxShape* box = static_cast<btBoxShape*>(rb.body->getCollisionShape());
                btVector3 extents = box->getHalfExtentsWithMargin();
                scale = { extents.x() * 2.0f, extents.y() * 2.0f, extents.z() * 2.0f };
            }
            break;
        }
        case RigidBodyComponent::Sphere:
        {
            model = sphereModel;
            if (rb.body->getCollisionShape()->getShapeType() == SPHERE_SHAPE_PROXYTYPE)
            {
                btSphereShape* sphere = static_cast<btSphereShape*>(rb.body->getCollisionShape());
                float radius = sphere->getRadius();
                float margin = sphere->getMargin();

                // 🔹 Получаем радиус debug-модели
                // Если у AssimpModelClass нет GetBoundingSphereRadius(), добавьте:
                // float modelRadius = sphereModel->GetBoundingSphereRadius();

                // ВРЕМЕННО: жёстко задайте радиус вашей модели
                constexpr float DEBUG_SPHERE_RADIUS = 1.732f; // ← Из вашего лога!

                // 🔹 Правильный масштаб = радиус Bullet / радиус модели
                float totalRadius = radius + margin;
                scale = {
                    totalRadius / DEBUG_SPHERE_RADIUS,
                    totalRadius / DEBUG_SPHERE_RADIUS,
                    totalRadius / DEBUG_SPHERE_RADIUS
                };

                char buf[256];
                sprintf_s(buf,
                    "Bullet radius: %.3f | Model radius: %.3f | Scale: %.3f\n",
                    totalRadius, DEBUG_SPHERE_RADIUS, scale.x
                );
                OutputDebugStringA(buf);
            }
            break;
        }
        case RigidBodyComponent::Capsule:
        {
            model = capsuleModel;
            if (rb.body->getCollisionShape()->getShapeType() == CAPSULE_SHAPE_PROXYTYPE)
            {
                btCapsuleShape* capsule = static_cast<btCapsuleShape*>(rb.body->getCollisionShape());
                float radius = capsule->getRadius();
                float halfHeight = capsule->getHalfHeight();
                float margin = capsule->getMargin();
                scale = { radius + margin, halfHeight * 2.0f + margin * 2.0f, radius + margin };
            }
            break;
        }
        default:
            continue;
        }

        if (!model) continue;

        // 🔹 ЕДИНСТВЕННЫЙ вызов SetScale — ПОСЛЕ Initialize
        GameObject marker;
        marker.Initialize(model, pos.x, pos.y, pos.z);
        marker.SetScale(scale.x, scale.y, scale.z);
        marker.SetRotation(rot.x, rot.y, rot.z);
        marker.RenderSimple(context, view, projection, textureShader);
    }
}


// ============================================================================
// RenderCollisionShapes для ColorShaderClass (ИСПРАВЛЕННАЯ ВЕРСИЯ)
// ============================================================================
void DebugPhysicsVisualizer::RenderCollisionShapes(
    ECSRegistry& registry,
    ID3D11DeviceContext* context,
    XMMATRIX view,
    XMMATRIX projection,
    AssimpModelClass* sphereModel,
    AssimpModelClass* boxModel,
    AssimpModelClass* capsuleModel,
    ColorShaderClass* colorShader,
    float alpha)
{
    if (!context || !colorShader) return;

    auto& rigidPool = registry.GetComponents<RigidBodyComponent>();
    auto& transformPool = registry.GetComponents<TransformComponent>();

    for (auto& [entity, rb] : rigidPool)
    {
        if (!registry.HasComponent<ActiveComponent>(entity)) continue;
        auto* transform = transformPool.Get(entity);
        if (!transform || !rb.body) continue;

        btCollisionShape* shape = rb.body->getCollisionShape();
        if (!shape) continue;

        btVector3 bulletPos = rb.body->getWorldTransform().getOrigin();
        XMFLOAT3 pos(bulletPos.x(), bulletPos.y(), bulletPos.z());
        XMFLOAT3 rot = transform->rotation;

        AssimpModelClass* model = nullptr;
        XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f };

        // 🔥 ВЫБИРАЕМ МОДЕЛЬ ПО РЕАЛЬНОЙ ФОРМЕ BULLET, а не по rb.shapeType
        switch (shape->getShapeType())
        {
        case SPHERE_SHAPE_PROXYTYPE:
        {
            model = sphereModel;
            btSphereShape* sphere = static_cast<btSphereShape*>(shape);
            float radius = sphere->getRadius();
            constexpr float DEBUG_SPHERE_RADIUS = 1.0f; // Подставь реальный радиус твоей модели
            float s = radius / DEBUG_SPHERE_RADIUS;
            scale = { s, s, s };
            break;
        }
        case BOX_SHAPE_PROXYTYPE:
        {
            model = boxModel;
            btBoxShape* box = static_cast<btBoxShape*>(shape);
            btVector3 extents = box->getHalfExtentsWithMargin();
            scale = { extents.x() * 2.0f, extents.y() * 2.0f, extents.z() * 2.0f };
            break;
        }
        case CAPSULE_SHAPE_PROXYTYPE:
        {
            model = capsuleModel;
            btCapsuleShape* capsule = static_cast<btCapsuleShape*>(shape);
            float radius = capsule->getRadius();
            float height = capsule->getHalfHeight() * 2.0f;
            scale = { radius, height, radius };
            break;
        }
        case CYLINDER_SHAPE_PROXYTYPE:
        {
            model = boxModel; // fallback или загрузи cylinderModel
            btCylinderShape* cyl = static_cast<btCylinderShape*>(shape);
            btVector3 extents = cyl->getHalfExtentsWithMargin();
            scale = { extents.x() * 2.0f, extents.y() * 2.0f, extents.z() * 2.0f };
            break;
        }
        case STATIC_PLANE_PROXYTYPE:
        {
            model = boxModel; // плоскость как тонкий бокс
            scale = { 10.0f, 0.01f, 10.0f };
            break;
        }
        case COMPOUND_SHAPE_PROXYTYPE:
        {
            // Для составных форм можно пропустить или нарисовать AABB
            continue;
        }
        default:
            // Неизвестная форма — пропускаем
            continue;
        }

        if (!model) continue;

        GameObject marker;
        marker.Initialize(model, pos.x, pos.y, pos.z);
        marker.SetScale(scale.x, scale.y, scale.z);
        marker.SetRotation(rot.x, rot.y, rot.z);
        marker.RenderSimpleColor(context, view, projection, colorShader);
    }
}

void DebugPhysicsVisualizer::RenderLogicalCompartments(
    ECSRegistry& registry,
    ID3D11DeviceContext* context,
    const XMMATRIX& view,
    const XMMATRIX& projection,
    ColorShaderClass* wireframeShader,
    AssimpModelClass* boxModel,  // ← 🔥 ДОБАВИТЬ параметр!
    const XMFLOAT4& colorDry,
    const XMFLOAT4& colorFlooded,
    float waterAlpha)
{
    if (!context || !wireframeShader || !boxModel) return;

    auto& compPool = registry.GetComponents<CompartmentComponent>();
    auto& transformPool = registry.GetComponents<TransformComponent>();
    auto& subPool = registry.GetComponents<SubmarineComponent>();

    // 🔥 ВАЖНО: Установить буферы ОДИН РАЗ перед циклом
    boxModel->Render(context);
    int indexCount = boxModel->GetIndexCount();

    for (auto& [entity, comp] : compPool)
    {
        // 🔥 ШАГ 1: Находим корневую подлодку
        auto* subLink = subPool.Get(entity);
        if (!subLink || subLink->submarineRoot == INVALID_ENTITY) continue;

        auto* rootTransform = transformPool.Get(subLink->submarineRoot);
        if (!rootTransform) continue;

        // 🔥 ШАГ 2: Вычисляем локальный центр из bounds
        XMFLOAT3 localCenter = {
            (comp.localMin.x + comp.localMax.x) * 0.5f,
            (comp.localMin.y + comp.localMax.y) * 0.5f,
            (comp.localMin.z + comp.localMax.z) * 0.5f
        };

        // 🔥 ШАГ 3: Преобразуем в мировые координаты
        XMMATRIX rootMatrix = rootTransform->GetWorldMatrix();
        XMVECTOR localCenterV = XMLoadFloat3(&localCenter);
        XMVECTOR worldCenterV = XMVector3TransformCoord(localCenterV, rootMatrix);
        XMFLOAT3 worldCenter;
        XMStoreFloat3(&worldCenter, worldCenterV);

        // 🔥 ШАГ 4: Вычисляем размеры бокса
        float sizeX = comp.localMax.x - comp.localMin.x;
        float sizeY = comp.localMax.y - comp.localMin.y;
        float sizeZ = comp.localMax.z - comp.localMin.z;

        // 🔥 ШАГ 5: Строим матрицу трансформации
        XMMATRIX world =
            XMMatrixScaling(sizeX, sizeY, sizeZ) *
            XMMatrixRotationRollPitchYaw(
                rootTransform->rotation.x,
                rootTransform->rotation.y,
                rootTransform->rotation.z) *
            XMMatrixTranslation(worldCenter.x, worldCenter.y, worldCenter.z);

        // 🔥 ШАГ 6: Цвет по затоплению
        float floodPercent = (comp.volume > 0.0f) ? (comp.waterVolume / comp.volume) : 0.0f;
        XMFLOAT4 color;

        if (floodPercent < 0.01f)
            color = colorDry;  // Зелёный
        else if (floodPercent < 0.5f)
            color = XMFLOAT4(1, 1, 0, 0.5f);  // Жёлтый
        else if (floodPercent < 0.9f)
            color = XMFLOAT4(1, 0.5f, 0, 0.6f);  // Оранжевый
        else
            color = colorFlooded;  // Красный

        // 🔥 ШАГ 7: Рендер (НЕ вызываем Render() повторно!)
        wireframeShader->Render(context, indexCount, world, view, projection);

        // 🔥 DEBUG
        char buf[256];
        sprintf_s(buf, "[DrawCompartment %u] World Y: %.2f, Size: (%.2f, %.2f, %.2f)\n",
            entity, worldCenter.y, sizeX, sizeY, sizeZ);
        OutputDebugStringA(buf);
    }
}