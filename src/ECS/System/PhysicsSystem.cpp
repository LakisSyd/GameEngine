
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define ENABLE_VHACD_IMPLEMENTATION 1  // ⚠️ Только в ОДНОМ .cpp файле!
#define USE_VHACD

// Теперь можно подключать заголовки
#include "PhysicsSystem.h"
#include "FloodingSystem.h"
#include "assimpmodelclass.h"
#include <BulletCollision/Gimpact/btGImpactShape.h>
#include "VHACD.h"
#include "PhysicsWorld.h"
struct VertexType
{
    XMFLOAT3 position;
    XMFLOAT2 texture;
    XMFLOAT3 normal;
    XMFLOAT3 tangent;  // ← нужен для TBN матрицы в pbr.vs
    XMFLOAT3 binormal; // ← нужен для TBN матрицы в pbr.vs
};

bool PhysicsSystem::InitializeWorld()
{
    return PhysicsWorld::Get().Initialize();
}

btCollisionShape* PhysicsSystem::CreateBoxShape(
    const XMFLOAT3& min, const XMFLOAT3& max, const XMFLOAT3& scale)
{
    float extentX = (max.x - min.x) * scale.x * 0.5f;
    float extentY = (max.y - min.y) * scale.y * 0.5f;
    float extentZ = (max.z - min.z) * scale.z * 0.5f;
    return new btBoxShape(btVector3(extentX, extentY, extentZ));
}

btCollisionShape* PhysicsSystem::CreateSphereShape(float radius, const XMFLOAT3& scale)
{
    float avgScale = (scale.x + scale.y + scale.z) / 3.0f;
    return new btSphereShape(radius * avgScale);
}

btCollisionShape* PhysicsSystem::CreateCylinderShape(
    float radius, float height, const XMFLOAT3& scale)
{
    btVector3 halfExtents(
        radius * scale.x,
        height * scale.y * 0.5f,
        radius * scale.z
    );
    return new btCylinderShape(halfExtents);
}

btCollisionShape* PhysicsSystem::CreateCapsuleShape(
    float radius, float height, const XMFLOAT3& scale)
{
    float scaledRadius = radius * max(scale.x, scale.z);
    float scaledHeight = height * scale.y - 2.0f * scaledRadius;
    return new btCapsuleShape(scaledRadius, scaledHeight);
}

btCollisionShape* PhysicsSystem::CreateConvexHullShape(
    AssimpModelClass* model, const XMFLOAT3& scale)
{
    if (!model) return nullptr;

    // 🔥 1. Создаём сам convex hull
    auto* convexHull = new btConvexHullShape();
    if (!convexHull) return nullptr;

    // 🔥 2. Получаем вершины из модели
    std::vector<XMFLOAT3> vertices;
    model->GetRawVertices(vertices);

    // 🔥 3. Защита от пустой модели
    if (vertices.empty())
    {
        OutputDebugStringA("[Physics] ConvexHull: no vertices, using fallback box\n");
        delete convexHull;
        return new btBoxShape(btVector3(0.5f, 0.5f, 0.5f));
    }

    // 🔥 4. Добавляем вершины с учётом масштаба
    for (const auto& v : vertices)
    {
        btVector3 point(
            v.x * scale.x,
            v.y * scale.y,
            v.z * scale.z
        );
        convexHull->addPoint(point, false); // false = не пересчитывать AABB на каждом шаге
    }

    // 🔥 5. КРИТИЧНО: финальная оптимизация и пересчёт AABB
    convexHull->recalcLocalAabb();
    convexHull->optimizeConvexHull();
    convexHull->setMargin(0.04f);

    // 🔥 6. Возвращаем созданную форму
    return convexHull;
}

btCollisionShape* PhysicsSystem::CreateTriangleMeshShape(
    AssimpModelClass* model, const XMFLOAT3& scale, RigidBodyComponent* rb)
{
    if (!model || !rb) return nullptr;

    // 🔥 1. Создаём btTriangleMesh
    auto* triangleMesh = new btTriangleMesh();
    if (!triangleMesh) return nullptr;

    // 🔥 2. Получаем треугольники
    std::vector<XMFLOAT3> triangles;
    model->GetRawTriangles(triangles); // ← аналогичная функция для треугольников

    if (triangles.size() < 3)
    {
        OutputDebugStringA("[Physics] TriangleMesh: not enough triangles, using fallback\n");
        delete triangleMesh;
        rb->triangleMesh = nullptr;
        return new btBoxShape(btVector3(0.5f, 0.5f, 0.5f));
    }

    // 🔥 3. Добавляем треугольники (по 3 вершины)
    for (size_t i = 0; i + 2 < triangles.size(); i += 3)
    {
        btVector3 v0(triangles[i].x * scale.x, triangles[i].y * scale.y, triangles[i].z * scale.z);
        btVector3 v1(triangles[i + 1].x * scale.x, triangles[i + 1].y * scale.y, triangles[i + 1].z * scale.z);
        btVector3 v2(triangles[i + 2].x * scale.x, triangles[i + 2].y * scale.y, triangles[i + 2].z * scale.z);

        triangleMesh->addTriangle(v0, v1, v2, true); // true = useQuantizedAabbCompression
    }

    // 🔥 4. Создаём финальную форму
    auto* meshShape = new btBvhTriangleMeshShape(triangleMesh, true, true);
    meshShape->setMargin(0.04f);

    // 🔥 5. Сохраняем ссылку на triangleMesh в компоненте!
    rb->triangleMesh = triangleMesh;

    return meshShape;
}

// ══════════════════════════════════════════════════════════════════════════════
// 🔥 CreateGImpactMeshShape — DYNAMIC concave mesh collision
// Unlike btBvhTriangleMeshShape (static only), btGImpactMeshShape supports
// dynamic bodies with mass > 0. This allows objects to exist INSIDE the mesh
// (e.g. crew/cargo inside a submarine hull) while the hull itself collides
// with external objects.
// ══════════════════════════════════════════════════════════════════════════════
btCollisionShape* PhysicsSystem::CreateGImpactMeshShape(
    AssimpModelClass* model, const XMFLOAT3& scale, RigidBodyComponent* rb)
{
    if (!model || !rb) return nullptr;

    // 1. Create btTriangleMesh (same as TriangleMesh)
    auto* triangleMesh = new btTriangleMesh();
    if (!triangleMesh) return nullptr;

    // 2. Get triangles from the model
    std::vector<XMFLOAT3> triangles;
    model->GetRawTriangles(triangles);

    if (triangles.size() < 3)
    {
        OutputDebugStringA("[Physics] GImpactMesh: not enough triangles, using fallback box\n");
        delete triangleMesh;
        rb->triangleMesh = nullptr;
        return new btBoxShape(btVector3(0.5f, 0.5f, 0.5f));
    }

    // 3. Add triangles with scale applied
    for (size_t i = 0; i + 2 < triangles.size(); i += 3)
    {
        btVector3 v0(triangles[i].x * scale.x, triangles[i].y * scale.y, triangles[i].z * scale.z);
        btVector3 v1(triangles[i + 1].x * scale.x, triangles[i + 1].y * scale.y, triangles[i + 1].z * scale.z);
        btVector3 v2(triangles[i + 2].x * scale.x, triangles[i + 2].y * scale.y, triangles[i + 2].z * scale.z);
        triangleMesh->addTriangle(v0, v1, v2, true);
    }

    // 4. Create GImpact shape — this is the KEY difference from TriangleMesh!
    //    btGImpactMeshShape supports dynamic bodies (mass > 0)
    auto* gimpactShape = new btGImpactMeshShape(triangleMesh);
    gimpactShape->setMargin(0.04f);
    gimpactShape->updateBound(); // CRITICAL: must call after creation!

    // 5. Store reference to triangle mesh for cleanup
    rb->triangleMesh = triangleMesh;

    char buf[256];
    sprintf_s(buf, "[Physics] GImpactMesh created: %zu triangles, scale (%.2f, %.2f, %.2f)\n",
        triangles.size() / 3, scale.x, scale.y, scale.z);
    OutputDebugStringA(buf);

    return gimpactShape;
}

void PhysicsSystem::RefreshRigidBody(ECSRegistry& registry, Entity entity, RigidBodyComponent* rb)
{
    
        if (!rb || !rb->body) return;

        auto* world = PhysicsWorld::Get().GetWorld();

        // 1. Сохраняем текущее состояние тела
        btTransform oldTransform;
        if (rb->body->getMotionState())
            rb->body->getMotionState()->getWorldTransform(oldTransform);
        else
            oldTransform = rb->body->getWorldTransform();

        btVector3 oldLinVel = rb->body->getLinearVelocity();
        btVector3 oldAngVel = rb->body->getAngularVelocity();



        // 2. Удаляем старое тело из мира
        world->removeRigidBody(rb->body);

        // 3. Очищаем память (но НЕ удаляем shape, если она используется в compound!)
        delete rb->body->getMotionState();
        delete rb->body;
        rb->body = nullptr;

        // triangleMesh удалять НЕ надо — он может использоваться в GImpact/TriangleMesh shapes
        // и удаляется в деструкторе RigidBodyComponent

        // 4. Создаём новое тело через существующую логику
        // ⚠️ CreateRigidBody сама добавит тело в мир с правильными масками
        PhysicsSystem::CreateRigidBody(registry, entity);

        // 5. Если тело создалось — восстанавливаем скорости (опционально)
        if (rb->body)
        {
            rb->body->setLinearVelocity(oldLinVel);
            rb->body->setAngularVelocity(oldAngVel);
        }
    
}

void PhysicsSystem::CreateRigidBody(ECSRegistry& registry, Entity entity)
{
    auto* rb = registry.GetComponent<RigidBodyComponent>(entity);
    auto* transform = registry.GetComponent<TransformComponent>(entity);
    auto* meshRenderer = registry.GetComponent<MeshRendererComponent>(entity);

    if (!rb || !transform) return;

    // ... (Проверки на NaN и масштаб оставляем без изменений) ...
    if (isnan(transform->position.x) || isnan(transform->position.y) || isnan(transform->position.z))
    {
        OutputDebugStringA("[ERROR] NaN in transform position! Resetting to (0,10,0)\n");
        transform->position = { 0.0f, 10.0f, 0.0f };
        transform->rotation = { 0.0f, 0.0f, 0.0f };
        transform->scale = { 1.0f, 1.0f, 1.0f };
    }

    if (transform->scale.x <= 0.0f || transform->scale.y <= 0.0f || transform->scale.z <= 0.0f)
    {
        OutputDebugStringA("[ERROR] Invalid scale! Resetting to (1,1,1)\n");
        transform->scale = { 1.0f, 1.0f, 1.0f };
    }

    btCollisionShape* shape = nullptr;

    if (auto* scs = registry.GetComponent<SubmarineCollisionSetup>(entity))
    {
        if (scs->useInteriorCollider && !rb->interiorBody)
        {
            // Размер с учётом глобального scale и shrink factor
            XMFLOAT3 size = scs->interiorSize;
            float shrink = scs->interiorShrinkFactor;
            XMFLOAT3 scaled = {
                size.x * shrink * transform->scale.x,
                size.y * shrink * transform->scale.y,
                size.z * shrink * transform->scale.z
            };

            btVector3 halfExtents(scaled.x * 0.5f, scaled.y * 0.5f, scaled.z * 0.5f);

            // Создаём форму и кинематическое/статическое тело (mass = 0)
            rb->interiorShape = new btBoxShape(halfExtents);

            btTransform interiorStart;
            interiorStart.setIdentity();
            interiorStart.setOrigin(btVector3(
                transform->position.x + scs->interiorOffset.x,
                transform->position.y + scs->interiorOffset.y,
                transform->position.z + scs->interiorOffset.z
            ));

            btDefaultMotionState* interiorMS = new btDefaultMotionState(interiorStart);
            btRigidBody::btRigidBodyConstructionInfo interiorCI(0.0f, interiorMS, rb->interiorShape, btVector3(0, 0, 0));
            rb->interiorBody = new btRigidBody(interiorCI);

            // Kinematic => будем вручную обновлять transform каждый кадр
            rb->interiorBody->setCollisionFlags(rb->interiorBody->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
            rb->interiorBody->setActivationState(DISABLE_DEACTIVATION);

            // Добавляем в мир отдельной группой (можно настроить маску)
            

            OutputDebugStringA("[Physics] Created submarine interior collider\n");
        }
    }

    // 🔥 ГЛАВНОЕ ИЗМЕНЕНИЕ: Логика выбора формы
    if (rb->isCompound && !rb->compoundElements.empty())
    {
        btCompoundShape* compoundShape = new btCompoundShape();

        for (const auto& elem : rb->compoundElements)
        {
            btCollisionShape* childShape = nullptr;

            // Создаем форму элемента с учетом смещений граней
            switch (elem.type)
            {
            case RigidBodyComponent::CompoundElement::Box:
            {
                // Базовый размер
                float hx = elem.size * 0.5f;
                float hy = elem.size * 0.5f;
                float hz = elem.size * 0.5f;

                // Применяем смещения граней (Offset)
                // Min offset уменьшает/увеличивает отрицательную сторону
                // Max offset уменьшает/увеличивает положительную сторону
                float minX = -hx + elem.boxOffsetMin.x;
                float maxX = hx + elem.boxOffsetMax.x;
                float minY = -hy + elem.boxOffsetMin.y;
                float maxY = hy + elem.boxOffsetMax.y;
                float minZ = -hz + elem.boxOffsetMin.z;
                float maxZ = hz + elem.boxOffsetMax.z;

                // Bullet требует симметричный бокс, поэтому берем макс диапазон
                // И компенсируем сдвигом центра локального трансформации
                float finalExtentX = (maxX - minX) * 0.5f;
                float finalExtentY = (maxY - minY) * 0.5f;
                float finalExtentZ = (maxZ - minZ) * 0.5f;

                childShape = new btBoxShape(btVector3(finalExtentX, finalExtentY, finalExtentZ));

                // ⚠️ ВАЖНО: Если границы несимметричны, нужно сдвинуть центр бокса
                // Вычисляем смещение центра относительно исходного центра элемента
                btVector3 centerShift(
                    (minX + maxX) * 0.5f,
                    (minY + maxY) * 0.5f,
                    (minZ + maxZ) * 0.5f
                );

                // Сохраняем этот сдвиг, чтобы применить его в transform ниже
                // (Временное решение: создадим отдельный трансформ для этого элемента)
                // Но проще создать shape, а потом добавить её с правильным локальным трансформом

                // Для простоты сейчас создадим shape без сдвига центра, 
                // а сдвиг границ просто меняет размер. 
                // Если нужна точная асимметрия - сложнее (требуется btConvexHull или сдвиг transform).
                // 👇 Упрощенный вариант: просто меняем размеры
                break;
            }

            case RigidBodyComponent::CompoundElement::VHACD:
            {
                // Попытка получить модель для VHACD из нескольких источников:
                // 1) явная ссылка elem.vhacdModel
                // 2) meshRenderer у текущей сущности (если есть)
                // 3) первая модель среди отсеков подлодки (SubmarineComponent::allCompartments)
                AssimpModelClass* vhacdModel = elem.vhacdModel;

                if (!vhacdModel)
                {
                    if (meshRenderer && meshRenderer->model)
                    {
                        vhacdModel = meshRenderer->model;
                    }
                    else
                    {
                        // Попробуем найти модель в отсеках у SubmarineComponent (если это подлодка)
                        auto* subComp = registry.GetComponent<SubmarineComponent>(entity);
                        if (subComp)
                        {
                            for (Entity compEnt : subComp->allCompartments)
                            {
                                auto* compMesh = registry.GetComponent<MeshRendererComponent>(compEnt);
                                if (compMesh && compMesh->model)
                                {
                                    vhacdModel = compMesh->model;
                                    break;
                                }
                            }
                        }
                    }
                }

                if (!vhacdModel)
                {
                    OutputDebugStringA("[Physics] VHACD: no model available for decomposition, using fallback box\n");
                    childShape = new btBoxShape(btVector3(0.5f, 0.5f, 0.5f));
                    break;
                }




                btTransform elemTransform;
                elemTransform.setIdentity();
                elemTransform.setOrigin(btVector3(elem.localPosition.x, elem.localPosition.y, elem.localPosition.z));
                elemTransform.setRotation(btQuaternion(
                    elem.localRotation.x, elem.localRotation.y, elem.localRotation.z, elem.localRotation.w));



            }
                
            

            case RigidBodyComponent::CompoundElement::Sphere:
            {
                float r = elem.radius + elem.radiusOffset;
                if (r < 0.01f) r = 0.01f;
                childShape = new btSphereShape(r);
                break;
            }

            case RigidBodyComponent::CompoundElement::Cylinder:
            {
                float r = elem.radius + elem.radiusOffset;
                float h = elem.height + elem.heightOffsetTop + elem.heightOffsetBottom;
                if (r < 0.01f) r = 0.01f;
                if (h < 0.01f) h = 0.01f;

                // Cylinder в Bullet ориентирован по Y
                childShape = new btCylinderShape(btVector3(r, h * 0.5f, r));
                break;
            }

            case RigidBodyComponent::CompoundElement::Capsule:
            {
                float r = elem.radius + elem.radiusOffset;
                float h = elem.height + elem.heightOffsetTop + elem.heightOffsetBottom;
                // Высота капсулы в Bullet - это высота цилиндрической части
                float cylHeight = h - 2.0f * r;
                if (cylHeight < 0.0f) cylHeight = 0.0f;

                childShape = new btCapsuleShape(r, cylHeight);
                break;
            }
            }

            if (childShape)
            {
                // Настраиваем локальный трансформ элемента
                btTransform childTrans;
                childTrans.setIdentity();

                // Позиция
                childTrans.setOrigin(btVector3(elem.localPosition.x, elem.localPosition.y, elem.localPosition.z));

                // Вращение (конвертация из кватерниона XMFLOAT4)
                childTrans.setRotation(btQuaternion(
                    elem.localRotation.x,
                    elem.localRotation.y,
                    elem.localRotation.z,
                    elem.localRotation.w
                ));

                // Добавляем в составную форму
                // true = управляем памятью дочерней формы сами (удалять будем в деструкторе CompoundShape)
                compoundShape->addChildShape(childTrans, childShape);
            }
            }

            shape = compoundShape;
        
    }
    else
    {
        // 3. Старая логика для одиночных форм
        switch (rb->shapeType)
        {
        case RigidBodyComponent::Sphere:
        {
            float radius = rb->shapeRadius;
            if (meshRenderer && meshRenderer->model && !rb->useManualSize)
            {
                XMFLOAT3 center;
                float meshRadius;
                meshRenderer->model->GetBoundingSphere(&center, &meshRadius);
                radius = meshRadius;
            }
            float avgScale = (transform->scale.x + transform->scale.y + transform->scale.z) / 3.0f;
            shape = new btSphereShape(radius * avgScale);
            break;
        }
        // ... (остальные кейсы Box, Cylinder, Capsule и т.д. остаются без изменений) ...
        case RigidBodyComponent::Box:
        {
            if (meshRenderer && meshRenderer->model)
            {
                XMFLOAT3 min, max;
                meshRenderer->model->GetBoundingBox(&min, &max);
                shape = CreateBoxShape(min, max, transform->scale);
            }
            else
            {
                shape = new btBoxShape(btVector3(
                    rb->shapeSize.x() * transform->scale.x * 0.5f,
                    rb->shapeSize.y() * transform->scale.y * 0.5f,
                    rb->shapeSize.z() * transform->scale.z * 0.5f
                ));
            }
            break;
        }
        case RigidBodyComponent::Cylinder:
        {
            float radius = rb->shapeRadius;
            float height = rb->shapeSize.y();
            shape = CreateCylinderShape(radius, height, transform->scale);
            break;
        }
        case RigidBodyComponent::Capsule:
        {
            float radius = rb->shapeRadius;
            float height = rb->shapeSize.y();
            shape = CreateCapsuleShape(radius, height, transform->scale);
            break;
        }
        case RigidBodyComponent::Plane:
        {
            float height = rb->shapeSize.y();
            shape = new btStaticPlaneShape(btVector3(0, 1, 0), height);
            break;
        }
        case RigidBodyComponent::ConvexHull:
        {
            if (meshRenderer && meshRenderer->model)
                shape = CreateConvexHullShape(meshRenderer->model, transform->scale);
            else
                return;
            break;
        }
        case RigidBodyComponent::TriangleMesh:
        {
            if (meshRenderer && meshRenderer->model)
            {
                shape = CreateTriangleMeshShape(meshRenderer->model, transform->scale, rb);
                if (rb->mass > 0.0f) rb->mass = 0.0f;
            }
            else
                return;
            break;
        }
        case RigidBodyComponent::GImpactMesh:
        {
            if (meshRenderer && meshRenderer->model)
            {
                shape = CreateGImpactMeshShape(meshRenderer->model, transform->scale, rb);
                // GImpactMesh SUPPORTS dynamic bodies — don't force mass to 0!
                OutputDebugStringA("[Physics] GImpactMesh shape created (dynamic concave)\n");
            }
            else
                return;
            break;
        }
        default:
            return;
        }
    }

    if (!shape) return;
    shape->setMargin(0.04f);

    // Расчет инерции
    btVector3 localInertia(0, 0, 0);
    if (rb->mass > 0.0f)
    {
        shape->calculateLocalInertia(rb->mass, localInertia);
        if (isnan(localInertia.getX()) || isnan(localInertia.getY()) || isnan(localInertia.getZ()))
        {
            OutputDebugStringA("[ERROR] NaN in inertia! Using fallback (1,1,1)\n");
            localInertia = btVector3(1, 1, 1);
        }
    }

    // Создание тела
    btTransform startTransform;
    startTransform.setIdentity();
    startTransform.setOrigin(btVector3(transform->position.x, transform->position.y, transform->position.z));
    startTransform.setRotation(EulerToQuaternion(transform->rotation));

    auto* motionState = new btDefaultMotionState(startTransform);
    btRigidBody::btRigidBodyConstructionInfo ci(rb->mass, motionState, shape, localInertia);

    // Удаляем старое тело если было
    if (rb->body) {
        PhysicsWorld::Get().GetWorld()->removeRigidBody(rb->body);
        delete rb->body->getMotionState();
        delete rb->body;
    }

    rb->body = new btRigidBody(ci);

    // Настройки тела
    if (rb->mass > 0.0f && rb->enableCCD)
    {
        rb->body->setCcdMotionThreshold(1.0f);
        rb->body->setCcdSweptSphereRadius(0.2f);
    }

    rb->body->setActivationState(DISABLE_DEACTIVATION);
    rb->body->setDamping(0.01f, 0.95f);
    rb->body->setFriction(0.8f);
    rb->body->setRollingFriction(0.3f);
    rb->body->setSpinningFriction(0.1f);

    PhysicsWorld::Get().GetWorld()->addRigidBody(rb->body);

    rb->lastSyncedPosition = transform->position;
    rb->lastSyncedRotation = transform->rotation;

    OutputDebugStringA(rb->isCompound ? "[Physics] Compound RigidBody created\n" : "[Physics] Simple RigidBody created\n");
}

// ═══════════════════════════════════════════════════════════════════════════
// 🔥 HELPER: Проверка, изменился ли трансформ снаружи (редактор, скрипт)
// ═══════════════════════════════════════════════════════════════════════════
static bool TransformChangedExternally(const TransformComponent* t, const RigidBodyComponent& rb)
{
    // Сравниваем побитово — это безопасно, т.к. мы сами записали эти значения
    return (t->position.x != rb.lastSyncedPosition.x ||
        t->position.y != rb.lastSyncedPosition.y ||
        t->position.z != rb.lastSyncedPosition.z ||
        t->rotation.x != rb.lastSyncedRotation.x ||
        t->rotation.y != rb.lastSyncedRotation.y ||
        t->rotation.z != rb.lastSyncedRotation.z);
}

void PhysicsSystem::Update(ECSRegistry& registry, float deltaTime, float waterLevel)
{
    auto& buoyancyPool = registry.GetComponents<BuoyancyComponent>();
    auto& rigidBodies = registry.GetComponents<RigidBodyComponent>();
    auto& transforms = registry.GetComponents<TransformComponent>();

    // ═══════════════════════════════════════════════════════════════════════
    // 🔥 FIX: FORWARD SYNC (ECS → Bullet)
    //
    // РАНЬШЕ: Каждый кадр перезаписывали ВСЕ тела из ECS в Bullet.
    //   Это вызывало Euler→Quat→Euler jitter, создававший фантомные
    //   импульсы на джойнтах → джойнты мгновенно рвались.
    //
    // ТЕПЕРЬ:
    //   - STATIC/KINEMATIC тела: всегда sync ECS → Bullet (управляются вручную)
    //   - DYNAMIC тела: sync ТОЛЬКО если трансформ изменён снаружи
    //     (пользователь подвинул объект в редакторе). Иначе — не трогаем,
    //     физика сама управляет позицией.
    // ═══════════════════════════════════════════════════════════════════════
    for (auto& [entity, rb] : rigidBodies)
    {
        if (!rb.body) continue;
        auto* transform = transforms.Get(entity);
        if (!transform) continue;

        if (rb.mass > 0.0f)
        {
            // DYNAMIC BODY: синхронизируем ТОЛЬКО при внешнем изменении
            if (TransformChangedExternally(transform, rb))
            {
                btTransform bt;
                bt.setIdentity();
                bt.setOrigin(btVector3(
                    transform->position.x,
                    transform->position.y,
                    transform->position.z));
                bt.setRotation(EulerToQuaternion(transform->rotation));

                rb.body->setWorldTransform(bt);
                if (rb.body->getMotionState())
                    rb.body->getMotionState()->setWorldTransform(bt);

                // Сбрасываем скорости чтобы не было рывка
                rb.body->setLinearVelocity(btVector3(0, 0, 0));
                rb.body->setAngularVelocity(btVector3(0, 0, 0));
                rb.body->clearForces();

                rb.lastSyncedPosition = transform->position;
                rb.lastSyncedRotation = transform->rotation;

                OutputDebugStringA("[PhysicsSync] External transform change detected, teleporting body\n");
            }

            rb.body->activate();
        }
        else
        {
            // STATIC/KINEMATIC BODY: всегда синхронизируем из ECS
            btTransform bt;
            bt.setIdentity();
            bt.setOrigin(btVector3(
                transform->position.x,
                transform->position.y,
                transform->position.z));
            bt.setRotation(EulerToQuaternion(transform->rotation));

            rb.body->setWorldTransform(bt);
            if (rb.body->getMotionState())
                rb.body->getMotionState()->setWorldTransform(bt);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════
    // ПЛАВУЧЕСТЬ С УЧЁТОМ ОТСЕКОВ
    // ═══════════════════════════════════════════════════════════════════════
    for (auto& [entity, buoyancy] : buoyancyPool)
    {
        if (!registry.HasComponent<RigidBodyComponent>(entity))
            continue;

        auto* rigidBody = rigidBodies.Get(entity);
        auto* transform = transforms.Get(entity);

        if (!rigidBody || !rigidBody->body || !transform)
            continue;

        btVector3 pos = rigidBody->body->getWorldTransform().getOrigin();
        float objectY = pos.getY();

        // ═══════════════════════════════════════════════════════════════
        // Определяем, является ли entity корнем подлодки
        // ═══════════════════════════════════════════════════════════════
        bool isSubmarineRoot = false;
        auto* subComp = registry.GetComponent<SubmarineComponent>(entity);
        if (subComp && subComp->submarineRoot == entity && !subComp->allCompartments.empty())
        {
            isSubmarineRoot = true;
        }

        float effectiveWaterLevel = waterLevel;

        if (!isSubmarineRoot)
        {
            Entity compartmentEntity = FloodingSystem::FindCompartmentForObject(
                registry, XMFLOAT3(pos.x(), pos.y(), pos.z()));

            if (compartmentEntity == entity)
                compartmentEntity = INVALID_ENTITY;

            if (compartmentEntity != INVALID_ENTITY)
            {
                auto* comp = registry.GetComponent<CompartmentComponent>(compartmentEntity);
                auto* compTransform = registry.GetComponent<TransformComponent>(compartmentEntity);

                if (comp && compTransform)
                {
                    float compartmentBottomY = compTransform->position.y + comp->localMin.y;
                    effectiveWaterLevel = compartmentBottomY + comp->waterLevel;

                    if (comp->waterLevel < 0.01f)
                    {
                        rigidBody->body->setDamping(0.01f, 0.01f);
                        continue;
                    }
                }
            }
        }

        // ═══════════════════════════════════════════════════════════════
        // Для подлодки: считаем воду во всех отсеках и общий объём
        // ═══════════════════════════════════════════════════════════════
        float totalWaterInCompartments = 0.0f;
        float totalCompartmentVolume = 0.0f;
        if (isSubmarineRoot && subComp)
        {
            for (Entity compEntity : subComp->allCompartments)
            {
                auto* comp = registry.GetComponent<CompartmentComponent>(compEntity);
                if (comp)
                {
                    totalWaterInCompartments += comp->waterVolume;
                    totalCompartmentVolume += comp->volume;
                }
            }
        }

        float halfHeight = buoyancy.height * 0.5f;
        float topY = objectY + halfHeight;
        float bottomY = objectY - halfHeight;

        float rawSubmersion = 0.0f;
        if (bottomY >= effectiveWaterLevel)
        {
            rawSubmersion = 0.0f;
        }
        else if (topY <= effectiveWaterLevel)
        {
            rawSubmersion = 1.0f;
        }
        else
        {

        }
        {
            rawSubmersion = (effectiveWaterLevel - bottomY) / buoyancy.height;
            rawSubmersion = fmaxf(0.0f, fminf(1.0f, rawSubmersion));
        }

        float smoothingFactor = 0.3f;
        buoyancy.smoothedSubmersion =
            buoyancy.smoothedSubmersion * (1.0f - smoothingFactor) +
            rawSubmersion * smoothingFactor;

        float submersionRatio = buoyancy.smoothedSubmersion;

        if (submersionRatio > 0.01f)
        {
            // ═══════════════════════════════════════════════════════════
            // КЛЮЧЕВОЕ ИЗМЕНЕНИЕ: Физически корректная модель затопления
            //
            // Сухая подлодка: F_buoy = ρ·g·V·submersion (выталкивает)
            //                 F_grav = mass·g (тянет вниз) — Bullet делает сам
            //
            // Затопленная:    Вода внутри ДОБАВЛЕТ вес, но НЕ добавляет объём!
            //                 F_buoy остаётся прежней (внешний объём не меняется)
            //                 F_grav = (mass + waterMass)·g
            //
            // Раньше мы вычитали waterVolume из buoyancyVolume — это НЕПРАВИЛЬНО.
            // Корпус подлодки не сжимается от воды внутри. Объём снаружи остаётся
            // тем же. Но МАССА растёт!
            // ═══════════════════════════════════════════════════════════

            float gravity = 9.81f;

            // Объём для плавучести = ПОЛНЫЙ внешний объём (не зависит от воды внутри)
            float buoyancyForce = buoyancy.fluidDensity * gravity * buoyancy.volume *
                submersionRatio * buoyancy.buoyancyMultiplier;

            // Вес воды внутри отсеков (1000 кг/м³ × объём воды × g)
            // Эта сила ДОПОЛНИТЕЛЬНО тянет вниз (Bullet не знает о ней)
            float waterWeight = totalWaterInCompartments * 1000.0f * gravity;

            // Итоговая сила: плавучесть вверх, минус доп. вес воды вниз
            float netUpForce = buoyancyForce - waterWeight;

            // Защита от экстремальных значений
            float weight = rigidBody->mass * gravity;
            float maxForce = weight * 5.0f;
            netUpForce = fmaxf(-maxForce, fminf(netUpForce, maxForce));

            rigidBody->body->applyCentralForce(btVector3(0, netUpForce, 0));

            // ═══════════════════════════════════════════════════════════
            // Демпфирование — уменьшаем vertical drag чтобы затопленная
            // подлодка тонула быстрее, а не зависала
            // ═══════════════════════════════════════════════════════════
            btVector3 vel = rigidBody->body->getLinearVelocity();

            // Доля затопления (0.0 = сухая, 1.0 = полностью затоплена)
            float floodRatio = (totalCompartmentVolume > 0.01f)
                ? (totalWaterInCompartments / totalCompartmentVolume)
                : 0.0f;
            floodRatio = fminf(1.0f, floodRatio);

            // Чем больше затоплена — тем МЕНЬШЕ вертикальный drag (быстрее тонет)
            float verticalDragCoeff = 50.0f * submersionRatio * (1.0f - floodRatio * 0.8f);
            float horizontalDragCoeff = 5.0f * submersionRatio;

            btVector3 dragForce(
                -vel.x() * horizontalDragCoeff,
                -vel.y() * verticalDragCoeff,
                -vel.z() * horizontalDragCoeff
            );
            rigidBody->body->applyCentralForce(dragForce);

            // ═══════════════════════════════════════════════════════════
            // Пружинная стабилизация — ТОЛЬКО если подлодка НЕ тонет
            // Если затоплено > 30% — пружину выключаем!
            // ═══════════════════════════════════════════════════════════
            if (submersionRatio > 0.9f && floodRatio < 0.3f)
            {
                float depthFromSurface = effectiveWaterLevel - objectY;
                float targetDepth = buoyancy.height;

                float depthError = targetDepth - depthFromSurface;
                float springK = 2.0f * rigidBody->mass;
                float springForce = -springK * depthError;

                float maxSpring = weight * 0.5f;
                springForce = fmaxf(-maxSpring, fminf(springForce, maxSpring));

                rigidBody->body->applyCentralForce(btVector3(0, springForce, 0));
            }

            // Угловое демпфирование
            btVector3 angVel = rigidBody->body->getAngularVelocity();
            float angularDragCoeff = 20.0f * submersionRatio;
            rigidBody->body->applyTorque(-angVel * angularDragCoeff);

            float linearDrag = 0.8f * submersionRatio * (1.0f - floodRatio * 0.5f);
            float angularDrag = 0.9f * submersionRatio;
            rigidBody->body->setDamping(linearDrag, angularDrag);

            // ═══════════════════════════════════════════════════════════
            // 📊 DEBUG: Состояние плавучести подлодки
            // ═══════════════════════════════════════════════════════════
#ifdef _DEBUG
            if (isSubmarineRoot)
            {
                static int debugCounter = 0;
                if (++debugCounter % 120 == 0)  // Раз в ~2 секунды
                {
                    char dbg[512];
                    sprintf_s(dbg,
                        "[Buoyancy] Y=%.1f | flood=%.0f%% (%.1f/%.1f m³) | "
                        "F_buoy=%.0f N | F_water=%.0f N | net=%.0f N | "
                        "weight=%.0f N | vel_y=%.2f\n",
                        objectY,
                        floodRatio * 100.0f,
                        totalWaterInCompartments, totalCompartmentVolume,
                        buoyancyForce, waterWeight, netUpForce,
                        weight, vel.y());
                    OutputDebugStringA(dbg);
                }
            }
#endif
        }
        else
        {
            rigidBody->body->setDamping(0.01f, 0.01f);
        }
    }

    // Шаг физики
    PhysicsWorld::Get().Update(deltaTime);

    // ═══════════════════════════════════════════════════════════════════════
    // 🔥 FIX: REVERSE SYNC (Bullet → ECS) — только для DYNAMIC тел
    //    Сохраняем lastSynced чтобы на следующем кадре отличить
    //    изменение из редактора от изменения из физики.
    // ═══════════════════════════════════════════════════════════════════════
    for (auto& [entity, rb] : rigidBodies)
    {
        if (!rb.body) continue;
        if (rb.mass <= 0.0f) continue;  // Статические тела не двигаются

        auto* transform = transforms.Get(entity);
        if (!transform) continue;

        btTransform bt = rb.body->getWorldTransform();
        btVector3 pos = bt.getOrigin();
        btQuaternion rot = bt.getRotation();

        // 🔥 ПРОВЕРКА: не записываем NaN в трансформ
        if (isnan(pos.x()) || isnan(pos.y()) || isnan(pos.z()))
        {
            OutputDebugStringA("[PHYSICS] NaN from Bullet! Skipping sync, freezing body\n");
            rb.body->setLinearVelocity(btVector3(0, 0, 0));
            rb.body->setAngularVelocity(btVector3(0, 0, 0));
            continue; // Не обновляем трансформ, оставляем последнюю валидную позицию
        }

        // 🔥 ПРОВЕРКА: защита от "выстреливания" за пределы разумного
        if (fabs(pos.y()) > 10000.0f)
        {
            OutputDebugStringA("[PHYSICS] Position out of bounds! Resetting body\n");
            rb.body->setWorldTransform(btTransform::getIdentity());
            rb.body->setLinearVelocity(btVector3(0, 0, 0));
            continue;
        }

        transform->position = XMFLOAT3(pos.x(), pos.y(), pos.z());
        transform->rotation = QuaternionToEuler(rot); // ← Убедись, что эта функция тоже защищает от NaN

        rb.lastSyncedPosition = transform->position;
        rb.lastSyncedRotation = transform->rotation;
    }
}

btCollisionShape* PhysicsSystem::CreateCompoundElementShape(
    const RigidBodyComponent::CompoundElement& elem)
{
    switch (elem.type)
    {
    case RigidBodyComponent::CompoundElement::Box:
    {
        float halfSize = elem.size * 0.5f;

        // Применяем смещения граней
        // Новая половина размера = базовая половина + смещение
        // Важно: смещение может быть отрицательным (уменьшать грань)
        float hx = halfSize + elem.boxOffsetMax.x;
        float hy = halfSize + elem.boxOffsetMax.y;
        float hz = halfSize + elem.boxOffsetMax.z;

        // Учитываем смещение минимума (оно "съедает" размер с другой стороны)
        // Фактический центр может сместиться, но btBoxShape строится от центра.
        // Для точной настройки формы через смещения лучше просто менять итоговый размер:

        float finalHalfX = halfSize + (elem.boxOffsetMax.x - elem.boxOffsetMin.x) * 0.5f; // Упрощенная логика
        // Но правильнее для Box просто сложить влияния:
        // Итоговый полуразмер = (Max - Min) / 2. 
        // Если мы считаем, что size это полный размер, а оффсеты двигают границы:

        float minX = -halfSize + elem.boxOffsetMin.x;
        float maxX = halfSize + elem.boxOffsetMax.x;
        float minY = -halfSize + elem.boxOffsetMin.y;
        float maxY = halfSize + elem.boxOffsetMax.y;
        float minZ = -halfSize + elem.boxOffsetMin.z;
        float maxZ = halfSize + elem.boxOffsetMax.z;

        // Bullet требует положительные размеры
        float finalHX = fmaxf(0.01f, (maxX - minX) * 0.5f);
        float finalHY = fmaxf(0.01f, (maxY - minY) * 0.5f);
        float finalHZ = fmaxf(0.01f, (maxZ - minZ) * 0.5f);

        return new btBoxShape(btVector3(finalHX, finalHY, finalHZ));
    }
    case RigidBodyComponent::CompoundElement::Sphere:
    {
        return new btSphereShape(elem.radius);
    }
    case RigidBodyComponent::CompoundElement::Cylinder:
    {
        // Cylinder в Bullet ориентирован вдоль Y, нам может понадобиться ротация
        btCylinderShape* cyl = new btCylinderShape(btVector3(
            elem.radius,
            elem.height * 0.5f,
            elem.radius
        ));
        return cyl;
    }

    case RigidBodyComponent::CompoundElement::Capsule:
    {
        // Capsule также ориентирован вдоль Y
        return new btCapsuleShape(elem.radius, elem.height);
    }


    default:
        return new btBoxShape(btVector3(0.5f, 0.5f, 0.5f));
    }
}


