#pragma once
#include "ECSRegistry.h"
#include <DirectXMath.h>
#include <cmath>
#include "CollisionUtils.h"
#include "Components.h"
#include "ECSTypes.h"
class PhysicsWorld;
// ═══════════════════════════════════════════════════════════════════════════
// 🔥 COLLISION GROUPS & MASKS — битовые флаги для фильтрации
// ═══════════════════════════════════════════════════════════════════════════
enum CollisionGroups : short
{
    COLLISION_GROUP_DEFAULT = 1 << 0,  // 1  — мир, статика
    COLLISION_GROUP_DYNAMIC = 1 << 1,  // 2  — динамические объекты
    COLLISION_GROUP_SUBMARINE_EXT = 1 << 2,  // 4  — внешний корпус подлодки
    COLLISION_GROUP_SUBMARINE_INT = 1 << 3,  // 8  — внутренняя "стена" (невидима для игроков)
    COLLISION_GROUP_PLAYER = 1 << 4,  // 16 — игрок / персонажи
    COLLISION_GROUP_PROJECTILE = 1 << 5,  // 32 — пули, торпеды
    COLLISION_GROUP_TRIGGER = 1 << 6,  // 64 — триггеры, зоны
};

enum CollisionMasks : short
{
    // Игрок: видит всё, КРОМЕ внутренней коллизии подлодки
    COLLISION_MASK_PLAYER =
    COLLISION_GROUP_DEFAULT | COLLISION_GROUP_DYNAMIC |
    COLLISION_GROUP_SUBMARINE_EXT | COLLISION_GROUP_PROJECTILE | COLLISION_GROUP_TRIGGER,
    // ❌ Нет COLLISION_GROUP_SUBMARINE_INT — игрок проходит сквозь стены изнутри

// Внешний корпус: видит мир, но НЕ игроков (они внутри)
COLLISION_MASK_SUBMARINE_EXT =
COLLISION_GROUP_DEFAULT | COLLISION_GROUP_DYNAMIC | COLLISION_GROUP_PROJECTILE,

// Внутренняя коллизия: только для триггеров/воды, НЕ для физики игроков
COLLISION_MASK_SUBMARINE_INT = COLLISION_GROUP_TRIGGER | COLLISION_GROUP_SUBMARINE_INT,

// Дефолт: сталкивается со всем
COLLISION_MASK_ALL = -1,
};


using namespace DirectX;


// Хэшер для XMFLOAT3
struct XMF3Hash {
    size_t operator()(const XMFLOAT3& v) const noexcept {
        uint64_t a = std::hash<int64_t>()(static_cast<int64_t>(std::lround(v.x * 100000.0f)));
        uint64_t b = std::hash<int64_t>()(static_cast<int64_t>(std::lround(v.y * 100000.0f)));
        uint64_t c = std::hash<int64_t>()(static_cast<int64_t>(std::lround(v.z * 100000.0f)));
        return (a * 73856093u) ^ (b * 19349663u) ^ (c * 83492791u);
    }
};
struct XMF3Eq {
    bool operator()(const XMFLOAT3& a, const XMFLOAT3& b) const noexcept {
        return fabsf(a.x - b.x) < 1e-6f && fabsf(a.y - b.y) < 1e-6f && fabsf(a.z - b.z) < 1e-6f;
    }
};



inline btQuaternion EulerToQuaternion(const XMFLOAT3& eulerDegrees)
{
    XMVECTOR rad = XMVectorSet(
        XMConvertToRadians(eulerDegrees.x),
        XMConvertToRadians(eulerDegrees.y),
        XMConvertToRadians(eulerDegrees.z),
        0.0f);
    XMVECTOR quat = XMQuaternionRotationRollPitchYawFromVector(rad);
    XMFLOAT4 q;
    XMStoreFloat4(&q, quat);
    return btQuaternion(q.x, q.y, q.z, q.w);
}

inline XMFLOAT3 QuaternionToEuler(const btQuaternion& q)
{
    double q0 = q.w();
    double q1 = q.x();
    double q2 = q.y();
    double q3 = q.z();

    double sinr_cosp = 2.0 * (q0 * q1 + q2 * q3);
    double cosr_cosp = 1.0 - 2.0 * (q1 * q1 + q2 * q2);
    double roll = atan2(sinr_cosp, cosr_cosp);

    double sinp = 2.0 * (q0 * q2 - q3 * q1);
    double pitch;
    if (fabs(sinp) >= 1.0)
        pitch = copysign(XM_PI / 2.0, sinp);
    else
        pitch = asin(sinp);

    double siny_cosp = 2.0 * (q0 * q3 + q1 * q2);
    double cosy_cosp = 1.0 - 2.0 * (q2 * q2 + q3 * q3);
    double yaw = atan2(siny_cosp, cosy_cosp);

    return XMFLOAT3(
        XMConvertToDegrees(static_cast<float>(roll)),
        XMConvertToDegrees(static_cast<float>(pitch)),
        XMConvertToDegrees(static_cast<float>(yaw))
    );
}

class PhysicsSystem
{
public:
    static bool InitializeWorld();
    static void Update(ECSRegistry& registry, float deltaTime, float waterLevel);
    static btCollisionShape* CreateCompoundElementShape(const RigidBodyComponent::CompoundElement& elem);
    static void CreateRigidBody(ECSRegistry& registry, Entity entity);

    // 🔥 ДОБАВЬТЕ ЭТИ МЕТОДЫ:
    static btCollisionShape* CreateBoxShape(
        const XMFLOAT3& min, const XMFLOAT3& max, const XMFLOAT3& scale);



    static void UpdateCharacterPos(ECSRegistry& registry) {
        auto& controllerPool = registry.GetComponents<CharacterController>();
        auto& transformPool = registry.GetComponents<TransformComponent>();

        for (auto& [entity, controller] : controllerPool) {
            if (!controller.ghostObj) continue;

            // 🔥 Читаем позицию ИЗ Bullet → в Transform
            btTransform trans = controller.ghostObj->getWorldTransform();
            btVector3 pos = trans.getOrigin();

            if (auto* tf = transformPool.Get(entity)) {
                tf->position = XMFLOAT3(pos.x(), pos.y(), pos.z());
                // Опционально: поворот, если нужно
                // btQuaternion rot = trans.getRotation();
                // tf->rotation = ...;
            }
        }
    }

    static btCollisionShape* CreateSphereShape(float radius, const XMFLOAT3& scale);

    static btCollisionShape* CreateCylinderShape(
        float radius, float height, const XMFLOAT3& scale);

    static btCollisionShape* CreateCapsuleShape(
        float radius, float height, const XMFLOAT3& scale);

    static btCollisionShape* CreateConvexHullShape(
        AssimpModelClass* model, const XMFLOAT3& scale);

    static btCollisionShape* CreateTriangleMeshShape(
        AssimpModelClass* model, const XMFLOAT3& scale, RigidBodyComponent* rb);



    // 🔥 NEW: GImpact concave mesh — works with dynamic bodies (unlike TriangleMesh)
    // Perfect for submarine hulls where objects need to be INSIDE
    static btCollisionShape* CreateGImpactMeshShape(
        AssimpModelClass* model, const XMFLOAT3& scale, RigidBodyComponent* rb);

    static void RefreshRigidBody(ECSRegistry& registry, Entity entity, RigidBodyComponent* rb);
};