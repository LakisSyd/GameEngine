#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <DirectXMath.h>
#include "ECSRegistry.h"

// =============================================================================
// ОПЦИОНАЛЬНЫЙ СЛОЙ NAVMESH
// =============================================================================
//
// Этот заголовок — единственная точка контакта основного NPC AI с
// Recast/Detour. Заголовки самой библиотеки здесь намеренно НЕ подключаются.
// Поэтому весь остальной проект ничего не знает о dtNavMesh, rcHeightfield и
// прочих типах сторонней библиотеки.
//
// Как полностью отключить слой:
//   1. Поставить NPC_NAVMESH_ENABLED=0 в DX_FrameWork.vcxproj.
//   2. При желании удалить NpcNavMeshSystem.cpp из списка ClCompile.
//
// При выключенном флаге функции ниже становятся маленькими inline-заглушками.
// StateSystem получит false и автоматически продолжит прямое MoveTowards.
#ifndef NPC_NAVMESH_ENABLED
#define NPC_NAVMESH_ENABLED 0
#endif

struct NpcNavMeshDebugInfo
{
    bool compiledIn = NPC_NAVMESH_ENABLED != 0;
    bool roomMeshBuilt = false;
    bool floorOnlyFallback = false;
    Entity room = INVALID_ENTITY;
    int polygonCount = 0;
    int waypointCount = 0;
    int currentWaypoint = 0;
    bool dynamicAvoidanceActive = false;
    Entity dynamicObstacle = INVALID_ENTITY;
    DirectX::XMFLOAT3 avoidanceStart{};
    DirectX::XMFLOAT3 avoidancePoint{};
    std::string status = NPC_NAVMESH_ENABLED
        ? "NavMesh has not been queried yet"
        : "NavMesh layer disabled; direct movement fallback is active";
    std::vector<DirectX::XMFLOAT3> path;
    // Каждые две соседние точки образуют один отрезок. Такой простой формат
    // не протаскивает типы Recast/Detour наружу из отключаемого слоя.
    std::vector<DirectX::XMFLOAT3> meshEdges;
    std::vector<DirectX::XMFLOAT3> obstacleEdges;
};

struct NpcNavMeshDebugDrawSettings
{
    bool enabled = false;
    bool drawMesh = true;
    bool drawPath = true;
    bool drawObstacleAabbs = false;
    float heightOffset = 0.08f;
};

namespace NpcNavMeshSystem
{
#if NPC_NAVMESH_ENABLED
    // Возвращает true и следующую локальную точку пути, если Detour смог
    // построить путь внутри currentRoom. false НЕ считается фатальной ошибкой:
    // вызывающий код обязан использовать прямое движение как fallback.
    bool FindNextWaypoint(
        ECSRegistry& registry,
        Entity npc,
        Entity currentRoom,
        const DirectX::XMFLOAT3& start,
        const DirectX::XMFLOAT3& target,
        std::uint32_t requestRevision,
        DirectX::XMFLOAT3& outWaypoint);

    // Проецирует желаемую точку на тот же связный остров NavMesh, где стоит NPC.
    // Wander проверяет так случайную цель до начала движения и не пытается
    // бесконечно идти в мебель или за пределы голубой проходимой поверхности.
    bool FindReachablePoint(
        ECSRegistry& registry,
        Entity currentRoom,
        const DirectX::XMFLOAT3& start,
        const DirectX::XMFLOAT3& desiredPoint,
        DirectX::XMFLOAT3& outPoint);

    // Динамические предметы не запекаются в Recast: иначе сетку пришлось бы
    // перестраивать каждый кадр. Вместо этого функция прогнозирует ближайший
    // Bullet AABB и возвращает короткую локальную точку объезда.
    bool FindDynamicAvoidanceWaypoint(
        ECSRegistry& registry,
        Entity npc,
        Entity currentRoom,
        const DirectX::XMFLOAT3& start,
        const DirectX::XMFLOAT3& desiredWaypoint,
        DirectX::XMFLOAT3& outWaypoint);

    // Сбрасывает только путь одного NPC. Сам запечённый mesh комнаты остаётся.
    void InvalidatePath(Entity npc);

    // Сбрасывает все пути и запечённые комнаты. Следующий запрос построит их
    // заново. Нужен после изменения коллайдеров/границ комнаты в Play mode.
    void InvalidateAll();

    NpcNavMeshDebugInfo GetDebugInfo(Entity npc);
    NpcNavMeshDebugDrawSettings& GetDebugDrawSettings();
#else
    inline bool FindNextWaypoint(
        ECSRegistry&,
        Entity,
        Entity,
        const DirectX::XMFLOAT3&,
        const DirectX::XMFLOAT3&,
        std::uint32_t,
        DirectX::XMFLOAT3&)
    {
        return false;
    }

    inline bool FindReachablePoint(
        ECSRegistry&,
        Entity,
        const DirectX::XMFLOAT3&,
        const DirectX::XMFLOAT3&,
        DirectX::XMFLOAT3&)
    {
        return false;
    }

    inline bool FindDynamicAvoidanceWaypoint(
        ECSRegistry&,
        Entity,
        Entity,
        const DirectX::XMFLOAT3&,
        const DirectX::XMFLOAT3&,
        DirectX::XMFLOAT3&)
    {
        return false;
    }

    inline void InvalidatePath(Entity) {}
    inline void InvalidateAll() {}

    inline NpcNavMeshDebugInfo GetDebugInfo(Entity)
    {
        return NpcNavMeshDebugInfo{};
    }

    inline NpcNavMeshDebugDrawSettings& GetDebugDrawSettings()
    {
        static NpcNavMeshDebugDrawSettings settings;
        return settings;
    }
#endif
}
