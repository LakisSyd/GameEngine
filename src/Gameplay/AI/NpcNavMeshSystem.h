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
    // true, если проект собран с Recast/Detour; false означает прямой fallback.
    bool compiledIn = NPC_NAVMESH_ENABLED != 0;
    // Сетка нужной комнаты/мира уже создана и доступна для запроса пути.
    bool roomMeshBuilt = false;
    // Полная геометрия не собралась, поэтому использован простой пол комнаты.
    bool floorOnlyFallback = false;
    // Комната последнего запроса; INVALID_ENTITY означает общую сетку мира.
    Entity room = INVALID_ENTITY;
    // Количество полигонов в построенной сетке — быстрая проверка, что она не пуста.
    int polygonCount = 0;
    // Сколько точек содержит текущий путь NPC и какая из них сейчас следующая.
    int waypointCount = 0;
    int currentWaypoint = 0;
    // Временный локальный объезд динамического Bullet-объекта активен прямо сейчас.
    bool dynamicAvoidanceActive = false;
    // Entity предмета, который вызвал объезд.
    Entity dynamicObstacle = INVALID_ENTITY;
    // Начало объезда и выбранная боковая точка в мировых координатах.
    DirectX::XMFLOAT3 avoidanceStart{};
    DirectX::XMFLOAT3 avoidancePoint{};
    // Последний результат/ошибка человеческим языком для панели диагностики.
    std::string status = NPC_NAVMESH_ENABLED
        ? "NavMesh has not been queried yet"
        : "NavMesh layer disabled; direct movement fallback is active";
    // Полный текущий путь, используемый debug-отрисовкой.
    std::vector<DirectX::XMFLOAT3> path;
    // Каждые две соседние точки образуют один отрезок. Такой простой формат
    // не протаскивает типы Recast/Detour наружу из отключаемого слоя.
    std::vector<DirectX::XMFLOAT3> meshEdges;
    std::vector<DirectX::XMFLOAT3> obstacleEdges;
};

struct NpcNavMeshDebugDrawSettings
{
    // Главный выключатель всей отладочной геометрии навигации.
    bool enabled = false;
    // Показывать рёбра NavMesh.
    bool drawMesh = true;
    // Показывать рассчитанный путь выбранного NPC.
    bool drawPath = true;
    // Показывать AABB динамических препятствий, участвующих в объезде.
    bool drawObstacleAabbs = false;
    // Показывать авторские точки укрытия и их peek-смещения.
    bool drawCoverPoints = false;
    // Показывать лучи тестового динамического поиска укрытий:
    // красный — отклонённый кандидат, голубой — проверка peek,
    // зелёный — выбранная позиция.
    bool drawCoverQueries = false;
    // true рисует пути всех NPC, false — только выбранного в студии.
    bool allNpcs = true;
    // Вертикальный подъём линий над полом, чтобы они не мерцали с геометрией.
    float heightOffset = 0.08f;
};

namespace NpcNavMeshSystem
{
#if NPC_NAVMESH_ENABLED
    // Возвращает true и следующую локальную точку пути, если Detour смог
    // построить путь внутри currentRoom либо по отмеченным поверхностям сцены,
    // если currentRoom == INVALID_ENTITY. false НЕ считается фатальной ошибкой:
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

    // Сбрасывает путь одного NPC. Успешный NavMesh остаётся в кеше.
    void InvalidatePath(Entity npc);

    // Сбрасывает все пути, сетки комнат и общую сетку открытой сцены.
    // Следующий запрос построит их заново.
    void InvalidateAll();

    // Возвращает копию диагностики последнего запроса конкретного NPC.
    NpcNavMeshDebugInfo GetDebugInfo(Entity npc);
    // Возвращает общие переключатели отладочной отрисовки по ссылке.
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
