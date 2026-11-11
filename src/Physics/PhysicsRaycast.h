#pragma once

#include <btBulletDynamicsCommon.h>
#include <DirectXMath.h>
#include <cmath>

#include <ECSTypes.h>
#include <cameraclass.h>

struct PhysicsRaycastHit
{
    Entity entity = INVALID_ENTITY;
    const btCollisionObject* collisionObject = nullptr;
    btVector3 rayStart = btVector3(0.0f, 0.0f, 0.0f);
    btVector3 rayEnd = btVector3(0.0f, 0.0f, 0.0f);
    DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 normal = { 0.0f, 0.0f, 0.0f };
};

class IgnoreEntityRayCallback final : public btCollisionWorld::ClosestRayResultCallback
{
public:
    IgnoreEntityRayCallback(
        const btVector3& rayStart,
        const btVector3& rayEnd,
        const btCollisionObject* ignoredObject,
        Entity ignoredEntity)
        : ClosestRayResultCallback(rayStart, rayEnd)
        , m_ignoredObject(ignoredObject)
        , m_ignoredEntity(ignoredEntity)
    {
    }

    btScalar addSingleResult(
        btCollisionWorld::LocalRayResult& rayResult,
        bool normalInWorldSpace) override
    {
        const btCollisionObject* hitObject = rayResult.m_collisionObject;

        if (hitObject == m_ignoredObject ||
            static_cast<Entity>(hitObject->getUserIndex()) == m_ignoredEntity ||
            hitObject->getActivationState() == DISABLE_SIMULATION)
        {
            return 1.0f;
        }

        return ClosestRayResultCallback::addSingleResult(rayResult, normalInWorldSpace);
    }

private:
    const btCollisionObject* m_ignoredObject = nullptr;
    Entity m_ignoredEntity = INVALID_ENTITY;
};

class PhysicsRaycast
{
public:
    static bool CastFromCamera(
        CameraClass* camera,
        btDynamicsWorld* world,
        float distance,
        Entity ignoredEntity,
        const btCollisionObject* ignoredObject,
        PhysicsRaycastHit& outHit)
    {
        if (!camera)
            return false;
        return CastRay(camera->GetPosition(), camera->GetForward(), world,
            distance, ignoredEntity, ignoredObject, outHit);
    }

    static bool CastRay(
        const DirectX::XMFLOAT3& origin,
        const DirectX::XMFLOAT3& direction,
        btDynamicsWorld* world,
        float distance,
        Entity ignoredEntity,
        const btCollisionObject* ignoredObject,
        PhysicsRaycastHit& outHit)
    {
        outHit = {};
        if (!world || distance <= 0.0f)
            return false;
        using namespace DirectX;
        const XMVECTOR normalized = XMVector3Normalize(XMLoadFloat3(&direction));
        XMFLOAT3 forward{};
        XMStoreFloat3(&forward, normalized);
        if (!std::isfinite(forward.x) || !std::isfinite(forward.y) ||
            !std::isfinite(forward.z))
            return false;

        outHit.rayStart = btVector3(
            origin.x, origin.y, origin.z);
        outHit.rayEnd = btVector3(
            origin.x + forward.x * distance,
            origin.y + forward.y * distance,
            origin.z + forward.z * distance);

        IgnoreEntityRayCallback callback(
            outHit.rayStart,
            outHit.rayEnd,
            ignoredObject,
            ignoredEntity);
        world->rayTest(outHit.rayStart, outHit.rayEnd, callback);

        if (!callback.hasHit() || !callback.m_collisionObject)
            return false;

        outHit.collisionObject = callback.m_collisionObject;
        outHit.entity = static_cast<Entity>(outHit.collisionObject->getUserIndex());
        outHit.position = {
            callback.m_hitPointWorld.x(),
            callback.m_hitPointWorld.y(),
            callback.m_hitPointWorld.z()
        };
        outHit.normal = {
            callback.m_hitNormalWorld.x(),
            callback.m_hitNormalWorld.y(),
            callback.m_hitNormalWorld.z()
        };

        return outHit.entity != INVALID_ENTITY;
    }
};
