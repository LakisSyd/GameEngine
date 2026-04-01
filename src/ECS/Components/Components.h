#pragma once
#include <DirectXMath.h>
#include "assimpmodelclass.h"
#include "MaterialClass.h"
#include <btBulletDynamicsCommon.h>
#include "ECSTypes.h"
#include <vector>
using namespace DirectX;

struct TransformComponent {
    XMFLOAT3 position = { 0,0,0 };
    XMFLOAT3 rotation = { 0,0,0 };
    XMFLOAT3 scale = { 1,1,1 };

    XMMATRIX GetWorldMatrix() const
    {
        XMMATRIX scaleMatrix = XMMatrixScaling(scale.x, scale.y, scale.z);
        XMMATRIX rotX = XMMatrixRotationX(rotation.x);
        XMMATRIX rotY = XMMatrixRotationY(rotation.y);
        XMMATRIX rotZ = XMMatrixRotationZ(rotation.z);
        XMMATRIX rotationMatrix = rotX * rotY * rotZ;
        XMMATRIX translationMatrix = XMMatrixTranslation(position.x, position.y, position.z);
        return scaleMatrix * rotationMatrix * translationMatrix;
    }
};

struct LightComponent
{
    XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
    XMFLOAT3 offset = { 0.0f, 0.0f, 0.0f };
    float radius = 10.0f;
    bool enabled = true;
    bool isEditorGizmo = false;
    Entity attachedTo = INVALID_ENTITY;
};

struct MeshRendererComponent {
    AssimpModelClass* model = nullptr;
    MaterialClass* material = nullptr;
};

struct ActiveComponent {};

struct RigidBodyComponent
{
    btRigidBody* body = nullptr;
    float mass = 1.0f;
    bool isDynamic = true;
    bool enableCCD = false;
    float ccdMotionThreshold = 1.0f;
    float linearDamping = 0.7f;
    float angularDamping = 0.8f;
    float angularFactor = 0.5f;
    float friction = 0.0f;
    bool isKinematic = false;

    XMFLOAT3 lastSyncedPosition = { 0, 0, 0 };
    XMFLOAT3 lastSyncedRotation = { 0, 0, 0 };

    enum ShapeType {
        Box,
        Sphere,
        Cylinder,
        Capsule,
        Plane,
        ConvexHull,
        TriangleMesh
    };

    ShapeType shapeType = Box;
    btVector3 shapeSize = btVector3(0.5f, 0.5f, 0.5f);
    float shapeRadius = 0.5f;
    bool useManualSize = false;

    btTriangleMesh* triangleMesh = nullptr;
    std::vector<btConvexHullShape*> convexHulls;

    bool isCompound = false;
    
    struct CompoundElement {
        enum ShapeType { Box, Sphere, Cylinder, Capsule };
        ShapeType type = Box;

        // Размеры
        float size = 1.0f;   // Для Box (полная длина стороны)
        float radius = 0.5f;
        float height = 1.0f;
        // Для Box: сдвигают границы AABB относительно центра
        XMFLOAT3 boxOffsetMin = { 0,0,0 };
        XMFLOAT3 boxOffsetMax = { 0,0,0 };
        // Трансформация ОТНОСИТЕЛЬНО центра подлодки
        XMFLOAT3 localPosition = { 0,0,0 };
        XMFLOAT4 localRotation = { 0,0,0,1 }; // Кватернион (x,y,z,w)

        float radiusOffset = 0.0f;
        float heightOffsetTop = 0.0f;
        float heightOffsetBottom = 0.0f;

        // Для отладки/веса (физический вес считает сам Bullet через инерцию, 
        // но это поле может пригодиться для логики игры)
        float debugWeight = 1.0f;
    };
    
     std::vector<CompoundElement> compoundElements;

     ~RigidBodyComponent()
     {
         if (body)
         {
             // 1. Сначала получаем форму
             btCollisionShape* shape = body->getCollisionShape();

             // 2. Удаляем тело и motion state
             delete body->getMotionState();
             delete body;
             body = nullptr;

             // 3. ВАЖНО: Если это составная форма, она САМА удалит свои части.
             // Нам нужно удалить только саму compound форму, но НЕ её детей.
             if (shape && shape->getShapeType() == COMPOUND_SHAPE_PROXYTYPE)
             {
                 delete shape;
                 // Не удаляем convexHulls/triangleMesh вручную, если они были добавлены в compound!
                 // Очищаем векторы, чтобы не было висячих указателей, но без delete.
                 convexHulls.clear();
                 if (triangleMesh) { delete triangleMesh; triangleMesh = nullptr; }
             }
             else
             {
                 // Если это обычная форма, удаляем её стандартно
                 if (shape) delete shape;

                 // Очищаем ручные ресурсы для обычных форм
                 if (triangleMesh) { delete triangleMesh; triangleMesh = nullptr; }
                 for (auto* hull : convexHulls)
                 {
                     if (hull) delete hull;
                 }
                 convexHulls.clear();
             }
         }
         else
         {
             // Если тела нет (ошибка создания), но ресурсы есть - чистим их вручную
             if (triangleMesh) { delete triangleMesh; triangleMesh = nullptr; }
             for (auto* hull : convexHulls)
             {
                 if (hull) delete hull;
             }
             convexHulls.clear();
         }
     }
};

struct StaticColliderComponent
{
    btCollisionShape* shape = nullptr;
    btRigidBody* body = nullptr;
};

struct BuoyancyComponent
{
    float fluidDensity = 1025.0f;  // Морская вода
    float volume = 10.0f;
    float height = 5.0f;
    float buoyancyMultiplier = 1.0f;
    float smoothedSubmersion = 0.0f;

    // 🔥 НОВОЕ: Отслеживание изменений для пересчета физики
    float lastCalculatedVolume = -1.0f;  // -1 = не инициализировано
};

struct BreachData
{
    XMFLOAT3 localPosition = { 0, 0, 0 };
    float size = 0.5f;
    float flowRate = 0.0f;
    bool isSealed = false;
};

struct CompartmentComponent
{
    XMFLOAT3 localMin = { -1, -1, -1 };
    XMFLOAT3 localMax = { 1,  1,  1 };
    XMFLOAT3 localCenter = { 0, 0, 0 };
    float volume = 8.0f;

    float waterVolume = 0.0f;
    float waterLevel = 0.0f;
    std::vector<BreachData> breaches;

    bool isSealed = true;
    std::vector<Entity> connectedCompartments;

    bool hasPower = true;
    float pumpRate = 0.1f;
    float damageVisual = 0.0f;

    // 🔥 НОВОЕ: Кэш для отслеживания изменений
    XMFLOAT3 lastCachedMin = { -1, -1, -1 };
    XMFLOAT3 lastCachedMax = { 1, 1, 1 };
    float lastCachedVolume = -1.0f;

    void RecalculateVolume()
    {
        float dx = localMax.x - localMin.x;
        float dy = localMax.y - localMin.y;
        float dz = localMax.z - localMin.z;
        volume = dx * dy * dz;
    }

    // 🔥 НОВОЕ: Проверка, изменились ли размеры
    bool HasSizeChanged() const
    {
        return lastCachedMin.x != localMin.x || lastCachedMin.y != localMin.y ||
            lastCachedMin.z != localMin.z || lastCachedMax.x != localMax.x ||
            lastCachedMax.y != localMax.y || lastCachedMax.z != localMax.z;
    }

    void CacheSize()
    {
        lastCachedMin = localMin;
        lastCachedMax = localMax;
        lastCachedVolume = volume;
    }

    bool ContainsPoint(const XMFLOAT3& worldPos, const XMMATRIX& worldToLocalMatrix) const
    {
        XMVECTOR localPosVec = XMVector3Transform(XMLoadFloat3(&worldPos), worldToLocalMatrix);
        XMFLOAT3 localPos;
        XMStoreFloat3(&localPos, localPosVec);
        return (localPos.x >= localMin.x && localPos.x <= localMax.x &&
            localPos.y >= localMin.y && localPos.y <= localMax.y &&
            localPos.z >= localMin.z && localPos.z <= localMax.z);
    }

    bool ContainsPoint(const XMFLOAT3& worldPos, const TransformComponent* compartmentTransform) const
    {
        if (!compartmentTransform) return false;
        XMMATRIX worldToLocal = XMMatrixInverse(nullptr, compartmentTransform->GetWorldMatrix());
        return ContainsPoint(worldPos, worldToLocal);
    }
};

struct SubmarineComponent
{
    Entity submarineRoot = INVALID_ENTITY;
    int compartmentIndex = 0;
    bool isDetached = false;
    std::vector<Entity> allCompartments;

    // 🔥 НОВОЕ: Кэш полной массы и объема
    float cachedTotalMass = 0.0f;
    float cachedTotalVolume = 0.0f;

    // 🔥 НОВОЕ: Флаги для отслеживания изменений
    bool needsPhysicsUpdate = false;
    int compartmentCountLastFrame = -1;
};

struct PressureHullComponent
{
    float maxPressure = 202000.0f;
    float currentHealth = 100.0f;
    float repairRate = 5.0f;
    bool isLeaking = false;
    float leakIntensity = 0.0f;
    float lastDepth = 0.0f;
    float lastPressure = 0.0f;
};

struct SubmarineJointComponent
{
    Entity compartmentA = INVALID_ENTITY;
    Entity compartmentB = INVALID_ENTITY;
    btTypedConstraint* joint = nullptr;

    float breakForce = 50000.0f;
    float currentStress = 0.0f;
    bool isBroken = false;

    ~SubmarineJointComponent()
    {
        if (joint)
        {
            joint = nullptr;
        }
    }
};

struct JointConnectionComponent
{
    Entity targetEntity = INVALID_ENTITY;

    btVector3 pivotA = btVector3(0, 0, 0);
    btVector3 pivotB = btVector3(0, 0, 0);

    float breakForce = 150000.0f;
    bool disableCollisions = true;

    btTypedConstraint* joint = nullptr;
    bool isBroken = false;

    bool showGizmo = true;
    XMFLOAT4 gizmoColor = { 1.0f, 1.0f, 0.0f, 1.0f };
};
