#pragma once

#include <DirectXMath.h>
#include <vector>

#include "ECSTypes.h"
#include "ECSRegistry.h"
#include "Components.h"

using namespace DirectX;

class RoomCullingSystem
{
public:
    struct DebugStats
    {
        bool enabled = false;
        Entity cameraRoom = INVALID_ENTITY;
        int visibleRoomCount = 0;
        int visibilityTests = 0;
        int roomContainmentQueries = 0;
        int roomChecks = 0;
        int visibleEntities = 0;
        int hiddenEntities = 0;
    };

    static Entity FindRoomContainingPoint(ECSRegistry& registry, const XMFLOAT3& point);
    static void BuildVisibleRooms(
        ECSRegistry& registry,
        const XMFLOAT3& cameraPos,
        std::vector<Entity>& outVisibleRooms);
    static bool IsEntityVisibleFromRooms(
        ECSRegistry& registry,
        Entity entity,
        const std::vector<Entity>& visibleRooms);
    static void ResetDebugStats();
    static const DebugStats& GetDebugStats();

private:
    static DebugStats s_debugStats;
};
