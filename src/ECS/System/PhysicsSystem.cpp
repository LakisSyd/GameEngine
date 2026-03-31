#include "PhysicsSystem.h"
#include "FloodingSystem.h"
#include "assimpmodelclass.h"


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
void PhysicsSystem::CreateRigidBody(ECSRegistry& registry, Entity entity)
{
    auto* rb = registry.GetComponent<RigidBodyComponent>(entity);
    auto* transform = registry.GetComponent<TransformComponent>(entity);
    auto* meshRenderer = registry.GetComponent<MeshRendererComponent>(entity);

    if (!rb || !transform) return;

    if (isnan(transform->position.x) || isnan(transform->position.y) || isnan(transform->position.z))
    {
        OutputDebugStringA("[ERROR] NaN in transform position! Resetting to (0,10,0)\n");
        transform->position = { 0.0f, 10.0f, 0.0f };
        transform->rotation = { 0.0f, 0.0f, 0.0f };
        transform->scale = { 1.0f, 1.0f, 1.0f };
    }

    // 🔥 ПРОВЕРКА: масштаб не должен быть нулевым или отрицательным
    if (transform->scale.x <= 0.0f || transform->scale.y <= 0.0f || transform->scale.z <= 0.0f)
    {
        OutputDebugStringA("[ERROR] Invalid scale! Resetting to (1,1,1)\n");
        transform->scale = { 1.0f, 1.0f, 1.0f };
    }


    btCollisionShape* shape = nullptr;

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

    default:
        return;
    }

    // 🔥 После создания shape — проверь инерцию на NaN
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


    if (!shape) return;
    shape->setMargin(0.04f);

    if (rb->mass > 0.0f)
        shape->calculateLocalInertia(rb->mass, localInertia);

    btTransform startTransform;
    startTransform.setIdentity();
    startTransform.setOrigin(btVector3(
        transform->position.x,
        transform->position.y,
        transform->position.z
    ));
    startTransform.setRotation(EulerToQuaternion(transform->rotation));

    auto* motionState = new btDefaultMotionState(startTransform);
    btRigidBody::btRigidBodyConstructionInfo ci(
        rb->mass, motionState, shape, localInertia
    );
    rb->body = new btRigidBody(ci);

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

    // ═══════════════════════════════════════════════════════════════════
    // 🔥 FIX: Инициализируем кэш синхронизации.
    //    Это позволяет отличить "физика двигает тело" от
    //    "пользователь переместил тело в редакторе".
    // ═══════════════════════════════════════════════════════════════════
    rb->lastSyncedPosition = transform->position;
    rb->lastSyncedRotation = transform->rotation;

    OutputDebugStringA("RigidBody created successfully\n");
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

        Entity compartmentEntity = FloodingSystem::FindCompartmentForObject(
            registry, XMFLOAT3(pos.x(), pos.y(), pos.z()));

        if (compartmentEntity == entity)
            compartmentEntity = INVALID_ENTITY;

        float effectiveWaterLevel = waterLevel;

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

        float halfHeight = buoyancy.height * 0.5f;
        float topY = objectY + halfHeight;
        float bottomY = objectY - halfHeight;

        float surfaceZone = 0.15f;
        float rawSubmersion = 0.0f;

        if (bottomY >= effectiveWaterLevel + surfaceZone)
        {
            rawSubmersion = 0.0f;
        }
        else if (topY <= effectiveWaterLevel - surfaceZone)
        {
            rawSubmersion = 1.0f;
        }
        else
        {
            rawSubmersion = (effectiveWaterLevel - bottomY) / (surfaceZone * 2.0f);
            rawSubmersion = fmaxf(0.0f, fminf(1.0f, rawSubmersion));
            rawSubmersion = rawSubmersion * rawSubmersion * (3.0f - 2.0f * rawSubmersion);
        }

        float smoothingFactor = 0.1f;
        buoyancy.smoothedSubmersion =
            buoyancy.smoothedSubmersion * (1.0f - smoothingFactor) +
            rawSubmersion * smoothingFactor;

        float submersionRatio = buoyancy.smoothedSubmersion;

        if (submersionRatio > 0.01f)
        {
            float effectiveBuoyancyVolume = buoyancy.volume;
            if (registry.HasComponent<CompartmentComponent>(entity))
            {
                auto* compSelf = registry.GetComponent<CompartmentComponent>(entity);
                if (compSelf)
                {
                    effectiveBuoyancyVolume = buoyancy.volume - compSelf->waterVolume;
                    if (effectiveBuoyancyVolume < 0.0f) effectiveBuoyancyVolume = 0.0f;
                }
            }

            float buoyancyForce = buoyancy.fluidDensity * 9.81f * effectiveBuoyancyVolume *
                submersionRatio * buoyancy.buoyancyMultiplier;
            float maxAcceleration = 20.0f;
            float maxForce = rigidBody->mass * maxAcceleration;
            buoyancyForce = fminf(buoyancyForce, maxForce);
            rigidBody->body->applyCentralForce(btVector3(0, buoyancyForce, 0));

            btVector3 vel = rigidBody->body->getLinearVelocity();
            float dragCoeff = 10.0f * submersionRatio;
            btVector3 dragForce = -vel * dragCoeff;
            rigidBody->body->applyCentralForce(dragForce);

            btVector3 angVel = rigidBody->body->getAngularVelocity();
            rigidBody->body->applyTorque(-angVel * dragCoeff * 0.5f);

            float massFactor = 100.0f / rigidBody->mass;
            massFactor = fminf(massFactor, 5.0f);

            float linearDrag = 1.5f * submersionRatio * massFactor;
            float angularDrag = 0.8f * submersionRatio * massFactor;
            rigidBody->body->setDamping(linearDrag, angularDrag);
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

