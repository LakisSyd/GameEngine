#include "RoomCullingSystem.h"

#include <algorithm>
#include <unordered_map>

RoomCullingSystem::DebugStats RoomCullingSystem::s_debugStats{};

namespace
{
    constexpr float kRoomBoundaryPadding = 0.15f;
    constexpr float kRoomFloorRecessPadding = 2.0f;

    struct CachedRoomTransform
    {
        XMFLOAT3 position{};
        XMFLOAT3 rotation{};
        XMFLOAT3 scale{};
        XMFLOAT4X4 worldToLocal{};
    };

    struct DoorRooms
    {
        Entity roomA = INVALID_ENTITY;
        Entity roomB = INVALID_ENTITY;
    };

    struct CachedEntityRoom
    {
        Entity room = INVALID_ENTITY;
    };

    std::unordered_map<Entity, CachedRoomTransform> s_roomTransformCache;
    std::unordered_map<Entity, DoorRooms> s_doorRoomsCache;
    std::unordered_map<Entity, CachedEntityRoom> s_entityRoomCache;

    bool IsSame(const XMFLOAT3& lhs, const XMFLOAT3& rhs)
    {
        return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
    }

    XMMATRIX GetWorldToRoomLocal(Entity roomEntity, const TransformComponent& transform)
    {
        auto& cached = s_roomTransformCache[roomEntity];
        if (!IsSame(cached.position, transform.position) ||
            !IsSame(cached.rotation, transform.rotation) ||
            !IsSame(cached.scale, transform.scale))
        {
            cached.position = transform.position;
            cached.rotation = transform.rotation;
            cached.scale = transform.scale;
            XMStoreFloat4x4(&cached.worldToLocal, XMMatrixInverse(nullptr, transform.GetWorldMatrix()));
        }

        return XMLoadFloat4x4(&cached.worldToLocal);
    }

    bool IsPointInsideRoom(
        Entity roomEntity,
        const RoomComponent& room,
        const TransformComponent& transform,
        const XMFLOAT3& point)
    {
        const XMMATRIX worldToRoomLocal = GetWorldToRoomLocal(roomEntity, transform);
        const XMVECTOR pointLocalVec = XMVector3TransformCoord(
            XMLoadFloat3(&point), worldToRoomLocal);

        XMFLOAT3 pointLocal;
        XMStoreFloat3(&pointLocal, pointLocalVec);

        return
            pointLocal.x >= room.localMin.x - kRoomBoundaryPadding && pointLocal.x <= room.localMax.x + kRoomBoundaryPadding &&
            pointLocal.y >= room.localMin.y - kRoomFloorRecessPadding && pointLocal.y <= room.localMax.y + kRoomBoundaryPadding &&
            pointLocal.z >= room.localMin.z - kRoomBoundaryPadding && pointLocal.z <= room.localMax.z + kRoomBoundaryPadding;
    }

    bool ContainsRoom(const std::vector<Entity>& rooms, Entity room)
    {
        return std::find(rooms.begin(), rooms.end(), room) != rooms.end();
    }

    bool IsDoorVisibleFromRooms(ECSRegistry& registry, Entity entity, const std::vector<Entity>& visibleRooms)
    {
        const auto it = s_doorRoomsCache.find(entity);
        if (it == s_doorRoomsCache.end())
            return false;

        return ContainsRoom(visibleRooms, it->second.roomA) ||
            ContainsRoom(visibleRooms, it->second.roomB);
    }
}

Entity RoomCullingSystem::FindRoomContainingPoint(ECSRegistry& registry, const XMFLOAT3& point)
{
    s_debugStats.roomContainmentQueries++;
    Entity foundRoom = INVALID_ENTITY;

    registry.ForEach<RoomComponent>(
        [&](Entity roomEntity, RoomComponent& room)
        {
            if (foundRoom != INVALID_ENTITY)
                return;

            auto* roomTransform = registry.GetComponent<TransformComponent>(roomEntity);
            if (!roomTransform)
                return;

            s_debugStats.roomChecks++;
            if (IsPointInsideRoom(roomEntity, room, *roomTransform, point))
                foundRoom = roomEntity;
        });

    return foundRoom;
}

void RoomCullingSystem::BuildVisibleRooms(
    ECSRegistry& registry,
    const XMFLOAT3& cameraPos,
    std::vector<Entity>& outVisibleRooms)
{
    outVisibleRooms.clear();
    s_doorRoomsCache.clear();

    Entity currentRoom = FindRoomContainingPoint(registry, cameraPos);
    s_debugStats.cameraRoom = currentRoom;
    if (currentRoom == INVALID_ENTITY)
        return;

    auto addUniqueRoom = [&](Entity roomToAdd)
    {
        if (roomToAdd == INVALID_ENTITY)
            return;

        if (std::find(outVisibleRooms.begin(), outVisibleRooms.end(), roomToAdd) != outVisibleRooms.end())
            return;

        outVisibleRooms.push_back(roomToAdd);
    };

    addUniqueRoom(currentRoom);

    registry.ForEach<DoorComponent>(
        [&](Entity doorEntity, DoorComponent& door)
        {
            const DoorRooms rooms{ door.roomA, door.roomB };
            s_doorRoomsCache[doorEntity] = rooms;
            if (door.leftLeaf != INVALID_ENTITY)
                s_doorRoomsCache[door.leftLeaf] = rooms;
            if (door.rightLeaf != INVALID_ENTITY)
                s_doorRoomsCache[door.rightLeaf] = rooms;

            const bool doorAllowsVisibility =
                door.isOpen ||
                door.isBroken ||
                door.openProgress > 0.05f;

            if (!doorAllowsVisibility)
                return;

            if (door.roomA == currentRoom)
            {
                addUniqueRoom(door.roomB);
            }
            else if (door.roomB == currentRoom)
            {
                addUniqueRoom(door.roomA);
            }
        });

    s_debugStats.visibleRoomCount = static_cast<int>(outVisibleRooms.size());
}

bool RoomCullingSystem::IsEntityVisibleFromRooms(
    ECSRegistry& registry,
    Entity entity,
    const std::vector<Entity>& visibleRooms)
{
    s_debugStats.visibilityTests++;

    if (visibleRooms.empty())
    {
        s_debugStats.visibleEntities++;
        return true;
    }

    if (IsDoorVisibleFromRooms(registry, entity, visibleRooms))
    {
        s_debugStats.visibleEntities++;
        return true;
    }

    if (registry.HasComponent<ShipPanelVisualBatchComponent>(entity))
    {
        // Ship panel batches may span multiple rooms. Culling them by one
        // inferred room causes visible walls and floors to disappear.
        s_debugStats.visibleEntities++;
        return true;
    }

    auto* transform = registry.GetComponent<TransformComponent>(entity);
    if (!transform)
    {
        s_debugStats.visibleEntities++;
        return true;
    }

    XMFLOAT3 testPoint = transform->position;
    if (auto* mesh = registry.GetComponent<MeshRendererComponent>(entity))
    {
        if (mesh->model)
        {
            XMFLOAT3 localMin{};
            XMFLOAT3 localMax{};
            mesh->model->GetBoundingBox(&localMin, &localMax);

            const XMFLOAT3 localCenter = {
                (localMin.x + localMax.x) * 0.5f,
                (localMin.y + localMax.y) * 0.5f,
                (localMin.z + localMax.z) * 0.5f
            };

            XMVECTOR worldCenter = XMVector3TransformCoord(
                XMLoadFloat3(&localCenter),
                transform->GetWorldMatrix());
            XMStoreFloat3(&testPoint, worldCenter);
        }
    }

    Entity entityRoom = INVALID_ENTITY;
    const auto cachedRoomIt = s_entityRoomCache.find(entity);
    if (cachedRoomIt != s_entityRoomCache.end())
    {
        auto* room = registry.GetComponent<RoomComponent>(cachedRoomIt->second.room);
        auto* roomTransform = registry.GetComponent<TransformComponent>(cachedRoomIt->second.room);
        if (room && roomTransform)
        {
            s_debugStats.roomChecks++;
            if (IsPointInsideRoom(
                cachedRoomIt->second.room, *room, *roomTransform, testPoint))
            {
                entityRoom = cachedRoomIt->second.room;
            }
        }
    }

    if (entityRoom == INVALID_ENTITY)
    {
        entityRoom = FindRoomContainingPoint(registry, testPoint);
        if (entityRoom != INVALID_ENTITY)
            s_entityRoomCache[entity] = CachedEntityRoom{ entityRoom };
        else
            s_entityRoomCache.erase(entity);
    }
    if (entityRoom == INVALID_ENTITY)
    {
        s_debugStats.visibleEntities++;
        return true;
    }

    const bool visible =
        std::find(visibleRooms.begin(), visibleRooms.end(), entityRoom) != visibleRooms.end();
    if (visible)
        s_debugStats.visibleEntities++;
    else
        s_debugStats.hiddenEntities++;

    return visible;
}

void RoomCullingSystem::ResetDebugStats()
{
    s_debugStats = {};
    s_debugStats.enabled = true;
}

const RoomCullingSystem::DebugStats& RoomCullingSystem::GetDebugStats()
{
    return s_debugStats;
}
