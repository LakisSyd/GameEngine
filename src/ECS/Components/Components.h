#pragma once
#include <algorithm>
#include <DirectXMath.h>
#include "assimpmodelclass.h"
#include "MaterialClass.h"
#include <bullet/btBulletDynamicsCommon.h>
#include "ECSTypes.h"
#include <vector>
#include <string>
#include <utility>
#include <bullet/BulletCollision/CollisionDispatch/btGhostObject.h>        // btPairCachingGhostObject
#include <bullet/BulletDynamics/Character/btKinematicCharacterController.h> // btKinematicCharacterController
#include "WorldAreaComponent.h"
#include "PhysicsWorld.h"
using namespace DirectX;


enum class PlayerLocomotionState
{
    Idle,
    Walk,
    Run,
    Exhausted
};


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


struct InteractionComponent
{
    bool enabled = true;
    float maxDistance = 3.0f;
    std::string prompt = "Interact";
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
	AssimpModelClass* lowerLodModel = nullptr; // Новый указатель на модель с низким уровнем детализации
    MaterialClass* material = nullptr;
    std::string asyncModelPath;
    bool useAsyncMesh = false;
    bool castsShadow = true;
};




struct ActiveComponent {};

// Корневая сущность импортированного корабля.
// Выделяем и двигаем именно ее, а не отдельные стены/комнаты.
struct ShipRootComponent
{
    XMFLOAT3 lastSyncedPosition = { 0, 0, 0 };
    std::string sourcePath;
};

// Метка детали корабля: комната, коридор, дверь, коллизия и т.п.
// root нужен, чтобы редактор мог по клику на деталь выбрать весь корабль.
struct ShipPartComponent
{
    Entity root = INVALID_ENTITY;
};

// Правила выбора в редакторе. Детали корабля будут перенаправлять выбор на root,
// чтобы случайно не сдвинуть одну комнату отдельно от всего корабля.
struct EditorSelectionComponent
{
    bool selectable = true;
    Entity redirectTo = INVALID_ENTITY;
};

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
    XMFLOAT3 lastSyncedScale = { 1, 1, 1 };

    // Runtime bridge between ECS ActiveComponent and Bullet. PhysicsSystem
    // preserves the body's flags when an entity is disabled, freezes it and
    // restores the original state when the entity becomes active again.
    bool suspendedByActiveState = false;
    int collisionFlagsBeforeSuspend = 0;
    int activationStateBeforeSuspend = ACTIVE_TAG;

	AssimpModelClass* collisionModel = nullptr; // Исходная модель для коллизии (для динамических мешей)
    std::string collisionModelPath;
	bool useSeparateCollisionMesh = false; // Использовать ли отдельную модель для коллизии (для динамических мешей)

    enum ShapeType {
        Box,
        Sphere,
        Cylinder,
        Capsule,
        Plane,
        ConvexHull,
        TriangleMesh,
        GImpactMesh,
        VHACD, // Dynamic concave mesh — allows objects inside (container collision)
        StairRamp
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
            Box,           // 0 > "Box"
            Sphere,        // 1 > "Sphere"
            Cylinder,      // 2 > "Cylinder"
            Capsule,       // 3 > "Capsule"
            ConvexHull,    // 4 > "ConvexHull"
            VHACD,         // 5 > "VHACD"
            StairRamp      // 6 > smooth player collision for stairs
        };

        ShapeType type = Box;

        // Размеры
        float size = 1.0f;   // Для Box (полная длина стороны)
        float radius = 0.5f;
        float height = 1.0f;
        XMFLOAT3 rampSize = { 2.0f, 1.0f, 3.0f };
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

        // ?? Для VHACD-элементов: ссылка на модель для декомпозиции
        AssimpModelClass* vhacdModel = nullptr;
        // ?? Для VHACD-элементов: масштаб для коллизии
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

    // -- Запрет копирования ----------------------------------------------------
    RigidBodyComponent() = default;
    RigidBodyComponent(const RigidBodyComponent&) = delete;
    RigidBodyComponent& operator=(const RigidBodyComponent&) = delete;
    btRigidBody* interiorBody = nullptr;
    btCollisionShape* interiorShape = nullptr;

    // -- Move-семантика --------------------------------------------------------
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
        , shape(other.shape)
        , needsCollisionRebuild(other.needsCollisionRebuild)
        , lastSyncedPosition(other.lastSyncedPosition)
        , lastSyncedRotation(other.lastSyncedRotation)
        , lastSyncedScale(other.lastSyncedScale)
        , suspendedByActiveState(other.suspendedByActiveState)
        , collisionFlagsBeforeSuspend(other.collisionFlagsBeforeSuspend)
        , activationStateBeforeSuspend(other.activationStateBeforeSuspend)
        , collisionModel(other.collisionModel)
        , collisionModelPath(std::move(other.collisionModelPath))
        , useSeparateCollisionMesh(other.useSeparateCollisionMesh)
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
        other.shape = nullptr;
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
        shape = other.shape;
        needsCollisionRebuild = other.needsCollisionRebuild;
        lastSyncedPosition = other.lastSyncedPosition;
        lastSyncedRotation = other.lastSyncedRotation;
        lastSyncedScale = other.lastSyncedScale;
        suspendedByActiveState = other.suspendedByActiveState;
        collisionFlagsBeforeSuspend = other.collisionFlagsBeforeSuspend;
        activationStateBeforeSuspend = other.activationStateBeforeSuspend;
        collisionModel = other.collisionModel;
        collisionModelPath = std::move(other.collisionModelPath);
        useSeparateCollisionMesh = other.useSeparateCollisionMesh;
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
        other.shape = nullptr;
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
            auto physics = PhysicsWorld::Get().GetWorld();

            if (physics)
            {
                physics->removeRigidBody(body);
            }
            btCollisionShape* shapeLocal = body->getCollisionShape();

            delete body->getMotionState();
            delete body;
            body = nullptr;
            shape = nullptr;

            // Удаляем связанную форму, если это compound (оригинальная логика)
            if (shapeLocal && shapeLocal->getShapeType() == COMPOUND_SHAPE_PROXYTYPE)
            {
                auto* compound = static_cast<btCompoundShape*>(shapeLocal);
                for (int i = compound->getNumChildShapes() - 1; i >= 0; --i)
                {
                    btCollisionShape* childShape = compound->getChildShape(i);
                    compound->removeChildShapeByIndex(i);
                    delete childShape;
                }
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

    float capsuleRadius = 0.4f;
    float capsuleHeight = 1.4f;
    float capsuleCenterOffsetY = 1.1f;
    float stepHeight = 1.f;
    float jumpSpeed = 6.0f;
    float gravity = 15.0f;
    float maxSlope = 45.0f;

    bool isGrounded = false;
    bool suspendedByActiveState = false;

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
        , capsuleRadius(other.capsuleRadius)
        , capsuleHeight(other.capsuleHeight)
        , capsuleCenterOffsetY(other.capsuleCenterOffsetY)
        , stepHeight(other.stepHeight)
        , jumpSpeed(other.jumpSpeed)
        , gravity(other.gravity)
        , maxSlope(other.maxSlope)
        , isGrounded(other.isGrounded)
        , suspendedByActiveState(other.suspendedByActiveState)
        , lastSyncedPosition(other.lastSyncedPosition)
        , lastSyncedRotation(other.lastSyncedRotation)
    {
        other.ghostObj = nullptr;
        other.controller = nullptr;
        other.capsuleShape = nullptr;
    }

    CharacterController& operator=(CharacterController&& other) noexcept {
        if (this == &other) return *this;

        ResetRuntime();
        ghostObj = other.ghostObj;
        controller = other.controller;
        capsuleShape = other.capsuleShape;
        capsuleRadius = other.capsuleRadius;
        capsuleHeight = other.capsuleHeight;
        capsuleCenterOffsetY = other.capsuleCenterOffsetY;
        stepHeight = other.stepHeight;
        jumpSpeed = other.jumpSpeed;
        gravity = other.gravity;
        maxSlope = other.maxSlope;
        isGrounded = other.isGrounded;
        suspendedByActiveState = other.suspendedByActiveState;
        lastSyncedPosition = other.lastSyncedPosition;
        lastSyncedRotation = other.lastSyncedRotation;

        other.ghostObj = nullptr;
        other.controller = nullptr;
        other.capsuleShape = nullptr;

        return *this;
    }

    void ResetRuntime() noexcept
    {
        if (auto* world = PhysicsWorld::Get().GetWorld())
        {
            if (controller)
                world->removeAction(controller);
            if (ghostObj && ghostObj->getBroadphaseHandle())
                world->removeCollisionObject(ghostObj);
        }
        delete controller;
        controller = nullptr;
        delete ghostObj;
        ghostObj = nullptr;
        delete capsuleShape;
        capsuleShape = nullptr;
    }

    ~CharacterController() { ResetRuntime(); }
};
struct CharacterMovement
{
    XMFLOAT3 walkDirection = { 0, 0, 0 };  // ? Явная инициализация
    bool wantJump = false;                // ? По умолчанию — не прыгать

    float moveSpeed = 3.5f;
    float runSpeed = 6.0f;
    float rotationSpeed = 5.0f;
    bool wantsRun = false;
	bool isRunning = false;                 // ? По умолчанию — не бежит
    bool isMoving = false;                // ? По умолчанию — стоит
    bool isGrounded = false;              // ? По умолчанию — в воздухе (безопаснее) 
	bool isCrouching = false;             // ? По умолчанию — не приседает
    float currentSpeed = 0.0f;

    PlayerLocomotionState locomotionState = PlayerLocomotionState::Idle;

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

    // ?? НОВОЕ: Отслеживание изменений для пересчета физики
    float lastCalculatedVolume = -1.0f;  // -1 = не инициализировано
};


enum class DoorAnimationMode
{
    Vertical = 0,
    Sliding = 1
};

struct DoorComponent
{
    // Отсеки по обе стороны двери
    Entity compartmentA = INVALID_ENTITY;
    Entity compartmentB = INVALID_ENTITY;

    // Rooms on both sides of the door for atmosphere simulation
    Entity roomA = INVALID_ENTITY;
    Entity roomB = INVALID_ENTITY;

    // Состояние двери
    bool isOpen = false; // открыта ли дверь
    bool isBroken = false; // сломана — заблокирована в открытом положении

    // Скорость потока воды через открытую дверь (м?/с)
    // Чем больше — тем быстрее выравнивается уровень между отсеками
    float flowRate = 1.0f;

    // Требуется ли питание для открытия/закрытия (электрические затворы)
    bool requiresPower = false;

    // Анимационный прогресс открытия [0.0 = закрыта, 1.0 = полностью открыта]
    // FloodingSystem использует его как множитель flowRate
    float openProgress = 0.0f;

    // Скорость анимации открытия/закрытия (единиц в секунду)
    float openSpeed = 2.0f;

    // Old rotation data is kept for compatibility with saved scenes.
    float openAngle = 90.0f;
    XMFLOAT3 closedRotation = { 0.0f, 0.0f, 0.0f };
    bool rotationInitialized = false;

    // Vertical sliding animation. DoorSystem captures the closed position
    // on the first update and raises the door along world Y while opening.
    DoorAnimationMode animationMode = DoorAnimationMode::Vertical;
    float openHeight = 2.0f;
    XMFLOAT3 closedPosition = { 0.0f, 0.0f, 0.0f };
    bool positionInitialized = false;

    // Optional two-leaf sci-fi sliding door. If these are valid, DoorSystem
    // moves the leaves apart and leaves the frame/root entity in place.
    Entity leftLeaf = INVALID_ENTITY;
    Entity rightLeaf = INVALID_ENTITY;
    XMFLOAT3 leftClosedPosition = { 0.0f, 0.0f, 0.0f };
    XMFLOAT3 rightClosedPosition = { 0.0f, 0.0f, 0.0f };
    bool leafPositionsInitialized = false;
    XMFLOAT3 slideAxis = { 1.0f, 0.0f, 0.0f };
    float slideDistance = 1.75f;

    // -- Вспомогательные методы ------------------------------------------------

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


struct HullPanelComponent
{
    std::string panelName = "Panel";
    std::string roomName = "";
    Entity room = INVALID_ENTITY;

    float maxHealth = 100.0f;
    float health = 100.0f;
    float accumulatedDamage = 0.0f;

    // Incoming hull damage is divided by this value. Tougher alloys can raise
    // resistance without requiring weapon-specific checks in DamageSystem.
    float armorResistance = 5.0f;

    int breachCount = 0;
    int maxBreaches = 3;

    bool isExternal = true;
    bool isDestroyed = false;
    bool collisionDisabledWhenDestroyed = true;

    bool CanCreateBreach() const
    {
        return breachCount < maxBreaches;
    }

    float CalculateHullDamage(float incomingDamage) const
    {
        return incomingDamage / (std::max)(armorResistance, 0.01f);
    }

    float Health01() const
    {
        return maxHealth > 0.0f ? health / maxHealth : 0.0f;
    }
};

struct HullBreachComponent
{
    Entity panel = INVALID_ENTITY;
    Entity room = INVALID_ENTITY;

    XMFLOAT3 localPosition = { 0.0f, 0.0f, 0.0f };
    XMFLOAT3 localNormal = { 0.0f, 1.0f, 0.0f };
    XMFLOAT3 worldPosition = { 0.0f, 0.0f, 0.0f };

    float radius = 0.25f;
    float leakRate = 0.15f;
    bool sealed = false;
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

    // ?? НОВОЕ: Кэш для отслеживания изменений
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

    // ?? НОВОЕ: Проверка, изменились ли размеры
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
// ==============================================================================
// Конфигурация физического корпуса подлодки (толстые стены через btCompoundShape)
// Настраивается через редактор. Применяется в FinalizeSubmarinePhysics.
// ==============================================================================
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

    // ?? НОВОЕ: Кэш полной массы и объема
    float cachedTotalMass = 0.0f;
    float cachedTotalVolume = 0.0f;

    // ?? НОВОЕ: Флаги для отслеживания изменений
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

    // Rooms on both sides of the door for atmosphere simulation
    Entity roomA = INVALID_ENTITY;
    Entity roomB = INVALID_ENTITY;
    btTypedConstraint* joint = nullptr;
    bool registeredInPhysicsWorld = false;

    float breakForce = 50000.0f;
    float currentStress = 0.0f;
    bool isBroken = false;

    SubmarineJointComponent() = default;
    SubmarineJointComponent(const SubmarineJointComponent&) = delete;
    SubmarineJointComponent& operator=(const SubmarineJointComponent&) = delete;

    SubmarineJointComponent(SubmarineJointComponent&& other) noexcept
        : compartmentA(other.compartmentA)
        , compartmentB(other.compartmentB)
        , roomA(other.roomA)
        , roomB(other.roomB)
        , joint(other.joint)
        , registeredInPhysicsWorld(other.registeredInPhysicsWorld)
        , breakForce(other.breakForce)
        , currentStress(other.currentStress)
        , isBroken(other.isBroken)
    {
        other.joint = nullptr;
        other.registeredInPhysicsWorld = false;
    }

    SubmarineJointComponent& operator=(SubmarineJointComponent&& other) noexcept
    {
        if (this == &other) return *this;
        ResetJoint();
        compartmentA = other.compartmentA;
        compartmentB = other.compartmentB;
        roomA = other.roomA;
        roomB = other.roomB;
        joint = other.joint;
        registeredInPhysicsWorld = other.registeredInPhysicsWorld;
        breakForce = other.breakForce;
        currentStress = other.currentStress;
        isBroken = other.isBroken;
        other.joint = nullptr;
        other.registeredInPhysicsWorld = false;
        return *this;
    }

    void ResetJoint() noexcept
    {
        if (joint && registeredInPhysicsWorld)
        {
            if (auto* world = PhysicsWorld::Get().GetWorld())
                world->removeConstraint(joint);
        }
        registeredInPhysicsWorld = false;
        delete joint;
        joint = nullptr;
    }

    ~SubmarineJointComponent() { ResetJoint(); }
};

struct JointConnectionComponent
{
    Entity targetEntity = INVALID_ENTITY;

    btVector3 pivotA = btVector3(0, 0, 0);
    btVector3 pivotB = btVector3(0, 0, 0);

    float breakForce = 150000.0f;
    bool disableCollisions = true;

    btTypedConstraint* joint = nullptr;
    bool registeredInPhysicsWorld = false;
    bool isBroken = false;

    bool showGizmo = true;
    XMFLOAT4 gizmoColor = { 1.0f, 1.0f, 0.0f, 1.0f };

    JointConnectionComponent() = default;
    JointConnectionComponent(const JointConnectionComponent&) = delete;
    JointConnectionComponent& operator=(const JointConnectionComponent&) = delete;

    JointConnectionComponent(JointConnectionComponent&& other) noexcept
        : targetEntity(other.targetEntity)
        , pivotA(other.pivotA)
        , pivotB(other.pivotB)
        , breakForce(other.breakForce)
        , disableCollisions(other.disableCollisions)
        , joint(other.joint)
        , registeredInPhysicsWorld(other.registeredInPhysicsWorld)
        , isBroken(other.isBroken)
        , showGizmo(other.showGizmo)
        , gizmoColor(other.gizmoColor)
    {
        other.joint = nullptr;
        other.registeredInPhysicsWorld = false;
    }

    JointConnectionComponent& operator=(JointConnectionComponent&& other) noexcept
    {
        if (this == &other) return *this;
        ResetJoint();
        targetEntity = other.targetEntity;
        pivotA = other.pivotA;
        pivotB = other.pivotB;
        breakForce = other.breakForce;
        disableCollisions = other.disableCollisions;
        joint = other.joint;
        registeredInPhysicsWorld = other.registeredInPhysicsWorld;
        isBroken = other.isBroken;
        showGizmo = other.showGizmo;
        gizmoColor = other.gizmoColor;
        other.joint = nullptr;
        other.registeredInPhysicsWorld = false;
        return *this;
    }

    void ResetJoint() noexcept
    {
        if (joint && registeredInPhysicsWorld)
        {
            if (auto* world = PhysicsWorld::Get().GetWorld())
                world->removeConstraint(joint);
        }
        registeredInPhysicsWorld = false;
        delete joint;
        joint = nullptr;
    }

    ~JointConnectionComponent() { ResetJoint(); }
};

// ==============================================================================
// Повреждения меша — трещины рисуются прямо в PBR шейдере как decal.
// Каждая запись = одна трещина в ЛОКАЛЬНЫХ координатах отсека.
// ==============================================================================
struct DamageDecalComponent
{
    static constexpr int MAX_CRACKS = 8; // максимум совпадает с cbuffer в шейдере

    struct CrackEntry
    {
        XMFLOAT3 localPosition = { 0,0,0 };
        XMFLOAT3 localNormal = { 0,0,1 }; // нормаль поверхности в пространстве меша
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

// Marks the single visual mesh that contains all hull panels of one ship.
// Damage decals are collected from the individual panels and rendered on it.
struct ShipPanelVisualBatchComponent
{
    Entity shipRoot = INVALID_ENTITY;
    Entity room = INVALID_ENTITY;
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

struct SpawnObjectDesc
{
    AssimpModelClass* model = nullptr;
    MaterialClass* material = nullptr;
    std::string name = "Object";

    XMFLOAT3 position{ 0, 0, 0 };
    XMFLOAT3 rotation{ 0, 0, 0 };
    XMFLOAT3 scale{ 1, 1, 1 };

    bool active = true;
    bool addMeshRenderer = true;
    bool meshCastsShadow = true;
    bool addInteraction = false;
    InteractionComponent interaction{};

    bool addRigidBody = false;
    RigidBodyComponent rigidBody{};
};

struct InteractionEvent
{
    Entity interactor = INVALID_ENTITY; // Кто инициировал взаимодействие
    Entity target = INVALID_ENTITY;     // С каким объектом взаимодействие

};

struct RoomComponent
{
    std::string roomName = "Room";
    std::vector<Entity> objectsInRoom;
    std::vector<Entity> connectedRooms;
    XMFLOAT3 localMin = { -0.5f, -0.5f, -0.5f };
    XMFLOAT3 localMax = { 0.5f, 0.5f, 0.5f };
    float oxygen = 1.0f;
    float pressure = 1.0f;
    float temperature = 20.0f;

    float leakRate = 0.0f;
    bool isActive = true;
};

// Static collision geometry marked as walkable for the scene-wide NPC NavMesh.
struct NavMeshSurfaceComponent
{
    bool enabled = true;
};


#pragma once
#include <algorithm>

struct DamageableComponent
{
    float health = 100.0f;
    float maxHealth = 100.0f;
    bool isDestroyed = false;

    void Normalize()
    {
        health = std::clamp(health, 0.0f, maxHealth);
        isDestroyed = health <= 0.0f;
    }

    bool IsAlive() const
    {
        return health > 0.0f && !isDestroyed;
    }
};

// Opt-in per-entity damage immunity. Keeping this separate from a particular
// health component lets the editor use one switch for players, NPCs, hull
// panels and ordinary Damageable entities.
struct InvulnerableComponent
{
};


struct RoomMembershipComponent
{
    Entity room = INVALID_ENTITY;
};

// Marks a mesh as a local first-person visual. Camera-following meshes are used
// for arms; attached meshes (for example a weapon) leave followCamera disabled.
struct FirstPersonViewComponent
{
    Entity owner = INVALID_ENTITY;
    XMFLOAT3 cameraOffset = { 0.0f, -1.0f, -0.22f };
    XMFLOAT3 rotationOffset = { 0.0f, 3.14159265f, 0.0f };
    XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f };
    float pitchScale = 1.0f;
    float pitchOffset = 0.0f;
    float lookUpLimit = 65.0f;   // degrees
    float lookDownLimit = 70.0f; // degrees
    bool followCamera = true;
    bool syncAnimation = true;
    bool hideOwner = true;
      // Runtime-only Animation Studio state. Freezes the current arms anchor
      // so the free camera can move around and inspect the FP mesh.
      bool studioFreezeCameraAnchor = false;
      bool studioCameraAnchorCaptured = false;
      XMFLOAT3 studioCameraPosition = {};
      XMFLOAT3 studioCameraRotation = {};
};

// Saved gameplay camera framing for the local player. Offsets use camera yaw:
// X is right, Y is up, Z is forward.
struct PlayCameraComponent
{
    XMFLOAT3 firstPersonEyeOffset = { 0.0f, 1.6f, 0.15f };
    XMFLOAT3 thirdPersonTargetOffset = { 0.0f, 1.5f, 0.0f };
    float thirdPersonDistance = 4.0f;
    // Дальняя плоскость основной игровой камеры. Объекты за ней не попадают
    // в кадр и отсекаются тем же frustum culling, который использует renderer.
    float renderDistance = 15.0f;
};

// Attaches an entity transform to an animated bone on another mesh. The local
// transform is evaluated in bone space, which makes it suitable for weapons,
// magazines, flashlights and other socket-like objects.
struct BoneAttachmentComponent
{
    Entity parent = INVALID_ENTITY;
    std::string boneName = "mixamorig_RightHand";
    XMFLOAT3 localPosition = { 0.0f, 0.0f, 0.0f };
    XMFLOAT3 localRotation = { 0.0f, 0.0f, 0.0f };
    XMFLOAT3 localScale = { 1.0f, 1.0f, 1.0f };
    bool enabled = true;
};
