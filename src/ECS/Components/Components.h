#pragma once
#include <DirectXMath.h>
#include "assimpmodelclass.h"
#include "MaterialClass.h"
#include <btBulletDynamicsCommon.h>
#include "ECSTypes.h"
#include <vector>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>        // btPairCachingGhostObject
#include <BulletDynamics/Character/btKinematicCharacterController.h> // btKinematicCharacterController
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


struct NameComponent
{
    std::string name = "Entity";

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
    btCollisionShape* shape = nullptr;
    bool needsCollisionRebuild = true;
    XMFLOAT3 lastSyncedPosition = { 0, 0, 0 };
    XMFLOAT3 lastSyncedRotation = { 0, 0, 0 };

    enum ShapeType {
        Box,
        Sphere,
        Cylinder,
        Capsule,
        Plane,
        ConvexHull,
        TriangleMesh,
        GImpactMesh,
        VHACD// Dynamic concave mesh — allows objects inside (container collision)
    };

    ShapeType shapeType = Box;
    btVector3 shapeSize = btVector3(0.5f, 0.5f, 0.5f);
    float shapeRadius = 0.5f;
    bool useManualSize = false;

    btTriangleMesh* triangleMesh = nullptr;
    std::vector<btConvexHullShape*> convexHulls;

    bool isCompound = false;

    struct CompoundElement {
        enum ShapeType {
            Box,           // 0 → "Box"
            Sphere,        // 1 → "Sphere"
            Cylinder,      // 2 → "Cylinder"
            Capsule,       // 3 → "Capsule"
            ConvexHull,    // 4 → "ConvexHull"
            VHACD          // 5 → "VHACD" ← должен быть ПОСЛЕДНИМ!
        };

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

        XMFLOAT3 editorEulerDegrees = { 0.0f, 0.0f, 0.0f };
        bool editorEulerDirty = true; // true = нужно пересчитать из localRo

        // Для отладки/веса (физический вес считает сам Bullet через инерцию, 
        // но это поле может пригодиться для логики игры)
        float debugWeight = 1.0f;

        // 🔥 Для VHACD-элементов: ссылка на модель для декомпозиции
        AssimpModelClass* vhacdModel = nullptr;
        // 🔥 Для VHACD-элементов: масштаб для коллизии
        XMFLOAT3 vhacdScale = { 1.0f, 1.0f, 1.0f };



        // Параметры качества (можно вынести в отдельную структуру, если много)
        uint32_t vhacdResolution = 800;          // детализация вокселей
        uint32_t vhacdMaxHulls = 6;               // макс. количество выпуклых частей
        uint32_t vhacdMaxVertsPerHull = 32;        // вершин на часть
        float vhacdVolumeError = 0.05f;              // допустимая погрешность в %

        // Статус (для отображения в редакторе)
        int vhacdLastHullCount = 0;                 // сколько халлов сгенерировано в последний раз
        bool vhacdGenerationPending = false;

    };

    std::vector<CompoundElement> compoundElements;

    // ── Запрет копирования ────────────────────────────────────────────────────
    RigidBodyComponent() = default;
    RigidBodyComponent(const RigidBodyComponent&) = delete;
    RigidBodyComponent& operator=(const RigidBodyComponent&) = delete;
    btRigidBody* interiorBody = nullptr;
    btCollisionShape* interiorShape = nullptr;

    // ── Move-семантика ────────────────────────────────────────────────────────
    RigidBodyComponent(RigidBodyComponent&& other) noexcept
        : body(other.body)
        , mass(other.mass)
        , isDynamic(other.isDynamic)
        , enableCCD(other.enableCCD)
        , ccdMotionThreshold(other.ccdMotionThreshold)
        , linearDamping(other.linearDamping)
        , angularDamping(other.angularDamping)
        , angularFactor(other.angularFactor)
        , friction(other.friction)
        , isKinematic(other.isKinematic)
        , lastSyncedPosition(other.lastSyncedPosition)
        , lastSyncedRotation(other.lastSyncedRotation)
        , shapeType(other.shapeType)
        , shapeSize(other.shapeSize)
        , shapeRadius(other.shapeRadius)
        , useManualSize(other.useManualSize)
        , triangleMesh(other.triangleMesh)
        , convexHulls(std::move(other.convexHulls))
        , isCompound(other.isCompound)
        , compoundElements(std::move(other.compoundElements))
        , interiorBody(other.interiorBody)
        , interiorShape(other.interiorShape)
    {
        // Обнуляем указатели у источника — деструктор источника НЕ удалит ресурсы
        other.body = nullptr;
        other.triangleMesh = nullptr;
        other.interiorBody = nullptr;
        other.interiorShape = nullptr;
    }

    RigidBodyComponent& operator=(RigidBodyComponent&& other) noexcept
    {
        if (this == &other) return *this;

        // Сначала освобождаем текущие ресурсы
        DestroyResources();

        body = other.body;
        mass = other.mass;
        isDynamic = other.isDynamic;
        enableCCD = other.enableCCD;
        ccdMotionThreshold = other.ccdMotionThreshold;
        linearDamping = other.linearDamping;
        angularDamping = other.angularDamping;
        angularFactor = other.angularFactor;
        friction = other.friction;
        isKinematic = other.isKinematic;
        lastSyncedPosition = other.lastSyncedPosition;
        lastSyncedRotation = other.lastSyncedRotation;
        shapeType = other.shapeType;
        shapeSize = other.shapeSize;
        shapeRadius = other.shapeRadius;
        useManualSize = other.useManualSize;
        triangleMesh = other.triangleMesh;
        convexHulls = std::move(other.convexHulls);
        isCompound = other.isCompound;
        compoundElements = std::move(other.compoundElements);
        interiorBody = other.interiorBody;
        interiorShape = other.interiorShape;

        other.body = nullptr;
        other.triangleMesh = nullptr;
        other.interiorBody = nullptr;
        other.interiorShape = nullptr;

        return *this;
    }

    ~RigidBodyComponent()
    {
        DestroyResources();
    }

private:
    void DestroyResources()
    {
        if (body)
        {
            btCollisionShape* shapeLocal = body->getCollisionShape();

            delete body->getMotionState();
            delete body;
            body = nullptr;

            // Удаляем связанную форму, если это compound (оригинальная логика)
            if (shapeLocal && shapeLocal->getShapeType() == COMPOUND_SHAPE_PROXYTYPE)
            {
                delete shapeLocal;
                convexHulls.clear();
                if (triangleMesh) { delete triangleMesh; triangleMesh = nullptr; }
            }
            else
            {
                if (shapeLocal) delete shapeLocal;
                if (triangleMesh) { delete triangleMesh; triangleMesh = nullptr; }
                for (auto* hull : convexHulls)
                    if (hull) delete hull;
                convexHulls.clear();
            }

            // Если есть interiorBody, удаляем его тоже
            if (interiorBody)
            {
                // interiorBody может иметь свою motionState
                delete interiorBody->getMotionState();
                delete interiorBody;
                interiorBody = nullptr;
            }
            if (interiorShape)
            {
                delete interiorShape;
                interiorShape = nullptr;
            }
        }
        else
        {
            if (triangleMesh) { delete triangleMesh; triangleMesh = nullptr; }
            for (auto* hull : convexHulls)
                if (hull) delete hull;
            convexHulls.clear();

            if (interiorBody)
            {
                delete interiorBody->getMotionState();
                delete interiorBody;
                interiorBody = nullptr;
            }
            if (interiorShape)
            {
                delete interiorShape;
                interiorShape = nullptr;
            }
        }
    }
};


struct CharacterController
{
    btPairCachingGhostObject* ghostObj = nullptr;
    btKinematicCharacterController* controller = nullptr;
    btCapsuleShape* capsuleShape = nullptr;

    float stepHeight = 0.35f;
    float jumpSpeed = 6.0f;
    float gravity = 15.0f;
    float maxSlope = 45.0f;

    bool isGrounded = false;

    // Последняя синхронизированная позиция/ротация — для детекции изменений из редактора
    XMFLOAT3 lastSyncedPosition = { 0.0f, 0.0f, 0.0f };
    XMFLOAT3 lastSyncedRotation = { 0.0f, 0.0f, 0.0f };

    // Запрет копирования
    CharacterController() = default;
    CharacterController(const CharacterController&) = delete;
    CharacterController& operator=(const CharacterController&) = delete;

    // Move-семантика (только перенос указателей)
    CharacterController(CharacterController&& other) noexcept
        : ghostObj(other.ghostObj)
        , controller(other.controller)
        , capsuleShape(other.capsuleShape)
        , stepHeight(other.stepHeight)
        , jumpSpeed(other.jumpSpeed)
        , gravity(other.gravity)
        , maxSlope(other.maxSlope)
        , isGrounded(other.isGrounded)
        , lastSyncedPosition(other.lastSyncedPosition)
        , lastSyncedRotation(other.lastSyncedRotation)
    {
        other.ghostObj = nullptr;
        other.controller = nullptr;
        other.capsuleShape = nullptr;
    }

    CharacterController& operator=(CharacterController&& other) noexcept {
        if (this == &other) return *this;

        ghostObj = other.ghostObj;
        controller = other.controller;
        capsuleShape = other.capsuleShape;
        stepHeight = other.stepHeight;
        jumpSpeed = other.jumpSpeed;
        gravity = other.gravity;
        maxSlope = other.maxSlope;
        isGrounded = other.isGrounded;
        lastSyncedPosition = other.lastSyncedPosition;
        lastSyncedRotation = other.lastSyncedRotation;

        other.ghostObj = nullptr;
        other.controller = nullptr;
        other.capsuleShape = nullptr;

        return *this;
    }

    ~CharacterController()
    {
        if (ghostObj)
        {
            delete ghostObj;
            ghostObj = nullptr;
        }
        if(controller)
        {
            delete controller;
            controller = nullptr;
		}

        if(capsuleShape)
        {
            delete capsuleShape;
            capsuleShape = nullptr;
		}

       
    }

   
};
struct CharacterMovement
{
    XMFLOAT3 walkDirection = { 0, 0, 0 };  // ✅ Явная инициализация
    bool wantJump = false;                // ✅ По умолчанию — не прыгать

    float moveSpeed = 3.5f;
    float runSpeed = 6.0f;
    float rotationSpeed = 5.0f;

    bool isMoving = false;                // ✅ По умолчанию — стоит
    bool isGrounded = false;              // ✅ По умолчанию — в воздухе (безопаснее) 
    float currentSpeed = 0.0f;

    // Конструктор можно убрать — инициализация выше уже работает
    // Но если хочешь оставить — ок:
    CharacterMovement() = default;
};;

struct PlayerGameTag
{
    // Пустой компонент — служит только для идентификации игрока
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


struct DoorComponent
{
    // Отсеки по обе стороны двери
    Entity compartmentA = INVALID_ENTITY;
    Entity compartmentB = INVALID_ENTITY;

    // Состояние двери
    bool isOpen = false; // открыта ли дверь
    bool isBroken = false; // сломана — заблокирована в открытом положении

    // Скорость потока воды через открытую дверь (м³/с)
    // Чем больше — тем быстрее выравнивается уровень между отсеками
    float flowRate = 1.0f;

    // Требуется ли питание для открытия/закрытия (электрические затворы)
    bool requiresPower = false;

    // Анимационный прогресс открытия [0.0 = закрыта, 1.0 = полностью открыта]
    // FloodingSystem использует его как множитель flowRate
    float openProgress = 0.0f;

    // Скорость анимации открытия/закрытия (единиц в секунду)
    float openSpeed = 2.0f;

    // ── Вспомогательные методы ────────────────────────────────────────────────

    // Открыть дверь (с учётом питания и поломки)
    bool TryOpen(bool hasPower)
    {
        if (isBroken) return true; // сломана — уже открыта
        if (requiresPower && !hasPower) return false;
        isOpen = true;
        return true;
    }

    // Закрыть дверь
    bool TryClose(bool hasPower)
    {
        if (isBroken) return false; // сломана — не закрыть
        if (requiresPower && !hasPower) return false;
        isOpen = false;
        return true;
    }

    // Текущий множитель потока с учётом анимации
    float GetEffectiveFlowRate() const
    {
        if (isBroken || isOpen)
            return flowRate * openProgress;
        return 0.0f;
    }
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
// ══════════════════════════════════════════════════════════════════════════════
// Конфигурация физического корпуса подлодки (толстые стены через btCompoundShape)
// Настраивается через редактор. Применяется в FinalizeSubmarinePhysics.
// ══════════════════════════════════════════════════════════════════════════════
struct SubmarineHullConfig
{
    // Полный внешний размер корпуса (X=ширина, Y=высота, Z=длина)
    XMFLOAT3 hullExtents = { 3.0f, 2.5f, 10.0f };

    // Толщина стен (мин. 0.5м для предотвращения туннелирования)
    float wallThickness = 0.5f;

    // Подлодка кинематическая (true) или динамическая (false)
    bool isKinematic = false;

    // Визуализировать физический корпус в редакторе
    bool showDebugHull = true;
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
    Entity proxyEntity = INVALID_ENTITY;
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
            delete joint;
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

// ══════════════════════════════════════════════════════════════════════════════
// Повреждения меша — трещины рисуются прямо в PBR шейдере как decal.
// Каждая запись = одна трещина в ЛОКАЛЬНЫХ координатах отсека.
// ══════════════════════════════════════════════════════════════════════════════
struct DamageDecalComponent
{
    static constexpr int MAX_CRACKS = 8; // максимум совпадает с cbuffer в шейдере

    struct CrackEntry
    {
        XMFLOAT3 localPosition = { 0,0,0 }; // позиция в пространстве меша
        float    radius = 0.5f;       // радиус трещины (м)
        float    intensity = 0.0f;       // 0=нет, 1=максимум (зависит от flowRate)
        float    rotation = 0.0f;       // угол поворота текстуры трещины (рандом)
        float    _pad[2] = {};         // выравнивание до 32 байт для cbuffer
    };

    std::vector<CrackEntry> cracks;

    // Синхронизировать с BreachData отсека
    void SyncFromBreaches(const CompartmentComponent& comp)
    {
        cracks.resize(comp.breaches.size());
        for (size_t i = 0; i < comp.breaches.size(); ++i)
        {
            auto& b = comp.breaches[i];
            auto& c = cracks[i];
            c.localPosition = b.localPosition;
            c.radius = b.size * 1.5f;
            c.intensity = b.isSealed ? 0.0f : std::min(1.0f, b.flowRate * 0.8f);
        }
    }
};

// Добавить новый простой компонент для настройки "интерьера" подлодки
struct SubmarineCollisionSetup
{
    // Внешний размер внутренней коробки (м). Рекомендуется чуть меньше визуального корпуса.
    XMFLOAT3 interiorSize = { 2.8f, 2.3f, 9.5f };

    // Локальный оффсет внутр. бокса относительно корневой трансформации подлодки
    XMFLOAT3 interiorOffset = { 0.0f, 0.0f, 0.0f };

    // Использовать ли этот внутренний коллайдер
    bool useInteriorCollider = true;

    // Коэффициент усадки (0.9–0.98) — задайте 0.95 для ~5% меньше корпуса
    float interiorShrinkFactor = 0.95f;
};

