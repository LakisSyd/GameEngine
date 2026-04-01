#pragma once
#include "ECSRegistry .h"
#include "PhysicsWorld.h"
#include <DirectXMath.h>
#include <cmath>

#include "CollisionUtils.h"
using namespace DirectX;


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

    static btCollisionShape* CreateSphereShape(float radius, const XMFLOAT3& scale);

    static btCollisionShape* CreateCylinderShape(
        float radius, float height, const XMFLOAT3& scale);

    static btCollisionShape* CreateCapsuleShape(
        float radius, float height, const XMFLOAT3& scale);

    static btCollisionShape* CreateConvexHullShape(
        AssimpModelClass* model, const XMFLOAT3& scale);

    static btCollisionShape* CreateTriangleMeshShape(
        AssimpModelClass* model, const XMFLOAT3& scale, RigidBodyComponent* rb);

private:
    static btVector3 ConvertTobtVector3(const XMFLOAT3& v);


};



inline void RefreshRigidBody(ECSRegistry& registry, Entity entity, RigidBodyComponent* rb)
{
    if (!rb->body) return;

    // 1. Удаляем старое тело из мира
    PhysicsWorld::Get().GetWorld()->removeRigidBody(rb->body);

    // 2. Очищаем память
    delete rb->body->getMotionState();
    delete rb->body->getCollisionShape(); // 🔥 Важно: удаляем и shape!
    delete rb->body;
    rb->body = nullptr;

    // 3. Если был triangle mesh — тоже удаляем
    if (rb->triangleMesh)
    {
        delete rb->triangleMesh;
        rb->triangleMesh = nullptr;
    }

    // 4. Создаём новое тело с обновлёнными параметрами
    PhysicsSystem::CreateRigidBody(registry, entity);
}

