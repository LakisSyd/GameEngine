#pragma once

#include "Components.h"
#include "ECSRegistry.h"
#include "JobSystem.h"
#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <Windows.h>

class RoomSystem
{
public:
    static void Update(ECSRegistry& registry, float deltaTime, JobSystem* jobs = nullptr)
    {
        (void)deltaTime;

        if (!jobs)
        {
            UpdateSynchronously(registry);
            return;
        }

        // Не ставим новую задачу, пока не применён результат предыдущего кадра.
        if (s_membershipJobPending.exchange(true))
            return;

        auto snapshot = std::make_shared<Snapshot>();
        auto& rooms = registry.GetComponents<RoomComponent>();
        auto& transforms = registry.GetComponents<TransformComponent>();

        for (auto& [roomEntity, room] : rooms)
        {
            auto* roomTransform = transforms.Get(roomEntity);
            if (!roomTransform || !room.isActive)
                continue;

            RoomSnapshot roomSnapshot;
            roomSnapshot.entity = roomEntity;
            roomSnapshot.localMin = room.localMin;
            roomSnapshot.localMax = room.localMax;
            XMStoreFloat4x4(
                &roomSnapshot.worldToLocal,
                XMMatrixInverse(nullptr, roomTransform->GetWorldMatrix()));
            snapshot->rooms.push_back(roomSnapshot);
        }

        for (auto& [entity, transform] : transforms)
        {
            if (!IsTrackableEntity(registry, entity))
                continue;

            snapshot->trackedObjects.push_back({ entity, transform.position });
        }

        if (snapshot->rooms.empty())
        {
            s_membershipJobPending = false;
            return;
        }

        if (!jobs->SubmitJob([snapshot, &registry, jobs]()
            {
                auto result = std::make_shared<Result>();
                result->rooms.reserve(snapshot->rooms.size());

                // Worker использует только скопированные границы и позиции.
                for (const RoomSnapshot& room : snapshot->rooms)
                {
                    RoomMembership membership;
                    membership.roomEntity = room.entity;
                    const XMMATRIX worldToLocal = XMLoadFloat4x4(&room.worldToLocal);

                    for (const TrackedObjectSnapshot& object : snapshot->trackedObjects)
                    {
                        const XMVECTOR local = XMVector3TransformCoord(
                            XMLoadFloat3(&object.position), worldToLocal);
                        XMFLOAT3 point{};
                        XMStoreFloat3(&point, local);

                        if (point.x >= room.localMin.x && point.x <= room.localMax.x &&
                            point.y >= room.localMin.y - 2.0f && point.y <= room.localMax.y + 0.15f &&
                            point.z >= room.localMin.z && point.z <= room.localMax.z)
                        {
                            membership.objects.push_back(object.entity);
                        }
                    }

                    result->rooms.push_back(std::move(membership));
                }

                jobs->SubmitMainThread([result, &registry]()
                    {
                        // Менять ECS разрешено только в главном потоке.
                        for (const RoomMembership& membership : result->rooms)
                        {
                            if (auto* room = registry.GetComponent<RoomComponent>(membership.roomEntity))
                                room->objectsInRoom = membership.objects;
                        }

                        s_membershipJobPending = false;
                    });
            }))
        {
            s_membershipJobPending = false;
        }
    }

private:
    struct RoomSnapshot
    {
        Entity entity = INVALID_ENTITY;
        XMFLOAT3 localMin{};
        XMFLOAT3 localMax{};
        XMFLOAT4X4 worldToLocal{};
    };

    struct TrackedObjectSnapshot
    {
        Entity entity = INVALID_ENTITY;
        XMFLOAT3 position{};
    };

    struct Snapshot
    {
        std::vector<RoomSnapshot> rooms;
        std::vector<TrackedObjectSnapshot> trackedObjects;
    };

    struct RoomMembership
    {
        Entity roomEntity = INVALID_ENTITY;
        std::vector<Entity> objects;
    };

    struct Result
    {
        std::vector<RoomMembership> rooms;
    };

    inline static std::atomic_bool s_membershipJobPending = false;

    static void UpdateSynchronously(ECSRegistry& registry)
    {
        auto& rooms = registry.GetComponents<RoomComponent>();
        auto& transforms = registry.GetComponents<TransformComponent>();

        for (auto& [roomEntity, room] : rooms)
            room.objectsInRoom.clear();

        for (auto& [roomEntity, room] : rooms)
        {
            auto* roomTransform = transforms.Get(roomEntity);
            if (!roomTransform || !room.isActive)
                continue;

            for (auto& [entity, transform] : transforms)
            {
                if (entity == roomEntity || !IsTrackableEntity(registry, entity))
                    continue;

                if (IsPointInsideRoom(*roomTransform, room, transform.position))
                    room.objectsInRoom.push_back(entity);
            }
        }
    }

    static bool IsTrackableEntity(ECSRegistry& registry, Entity entity)
    {
        if (registry.HasComponent<CharacterController>(entity))
            return true;

        auto* rigidBody = registry.GetComponent<RigidBodyComponent>(entity);
        if (!rigidBody)
            return false;

        return rigidBody->isDynamic && !rigidBody->isKinematic && rigidBody->mass > 0.0f;
    }

    static bool IsPointInsideRoom(
        const TransformComponent& roomTransform,
        const RoomComponent& room,
        const XMFLOAT3& point)
    {
        XMMATRIX worldToLocal = XMMatrixInverse(nullptr, roomTransform.GetWorldMatrix());
        XMVECTOR localPointVec = XMVector3TransformCoord(XMLoadFloat3(&point), worldToLocal);
        XMFLOAT3 localPoint{};
        XMStoreFloat3(&localPoint, localPointVec);

        return localPoint.x >= room.localMin.x && localPoint.x <= room.localMax.x &&
            // Физический SHIP_PANEL может опускаться ниже логического room AABB.
            localPoint.y >= room.localMin.y - 2.0f && localPoint.y <= room.localMax.y + 0.15f &&
            localPoint.z >= room.localMin.z && localPoint.z <= room.localMax.z;
    }
};
