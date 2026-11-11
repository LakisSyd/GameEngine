#pragma once

#include <btBulletDynamicsCommon.h>
#include <DirectXMath.h>

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
            static_cast<Entity>(hitObject->getUserIndex()) == m_ignoredEntity)
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
        outHit = {};

        if (!camera || !world || distance <= 0.0f)
            return false;

        const DirectX::XMFLOAT3 cameraPosition = camera->GetPosition();
        const DirectX::XMFLOAT3 cameraForward = camera->GetForward();

        outHit.rayStart = btVector3(
            cameraPosition.x,
            cameraPosition.y,
            cameraPosition.z);
        outHit.rayEnd = btVector3(
            cameraPosition.x + cameraForward.x * distance,
            cameraPosition.y + cameraForward.y * distance,
            cameraPosition.z + cameraForward.z * distance);

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
