#include "NpcNavMeshSystem.h"

#if NPC_NAVMESH_ENABLED

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <windows.h>

#include <recastnavigation/Recast.h>
#include <recastnavigation/DetourNavMesh.h>
#include <recastnavigation/DetourNavMeshBuilder.h>
#include <recastnavigation/DetourNavMeshQuery.h>
#include <bullet/BulletCollision/CollisionShapes/btCompoundShape.h>
#include <bullet/BulletCollision/CollisionShapes/btConcaveShape.h>
#include <bullet/BulletCollision/CollisionShapes/btShapeHull.h>

// =============================================================================
// ВНУТРЕННОСТИ NAVMESH NPC — КАК ЧИТАТЬ ЭТОТ ФАЙЛ
// =============================================================================
//
//   Collect*Geometry()  -> собирает треугольники пола и коробки препятствий;
//   BuildRoomNavMesh()  -> Recast превращает геометрию в проходимые полигоны;
//   BuildPath()         -> Detour строит коридор и прямые waypoint-точки;
//   FindNextWaypoint()  -> отдаёт StateSystem только ближайшую нужную точку;
//   FindDynamicAvoidanceWaypoint() -> локально обходит незапечённый подвижный
//                                     Bullet-объект;
//   Invalidate*()       -> выкидывает устаревший путь/сетку из кеша.
//
// Координаты в публичном API мировые. Recast/Detour-типы намеренно остаются
// только в этом .cpp. Комната INVALID_ENTITY означает общую открытую сетку мира.
// Если любой этап не сработал, функция возвращает false, а StateSystem имеет
// право использовать прямое движение — отсутствие NavMesh не должно стопорить AI.
namespace
{
    // Флаг проходимого полигона и номер обычной наземной области Detour.
    constexpr unsigned short NAV_FLAG_WALK = 0x01;
    constexpr unsigned char NAV_AREA_GROUND = 0;
    // Жёсткие пределы временных массивов запроса защищают от бесконечного пути.
    constexpr int MAX_POLY_PATH = 256;
    constexpr int MAX_STRAIGHT_PATH = 64;

    // Сырьё для одного запуска Recast: вершины xyz, индексы треугольников,
    // тип каждой области и AABB препятствий для debug-отрисовки.
    struct BuildGeometry
    {
        struct ObstacleBox
        {
            // Минимальный и максимальный угол AABB в мировых координатах.
            float minimum[3]{};
            float maximum[3]{};
        };

        std::vector<float> vertices; // Плоский массив x,y,z,x,y,z...
        std::vector<int> indices; // По три индекса на треугольник.
        std::vector<unsigned char> areas; // Одна Recast area на треугольник.
        std::vector<ObstacleBox> obstacles; // Только для диагностики границ.
        int surfaceTriangleCount = 0; // Всего треугольников поверхностей.
        int walkableSurfaceTriangleCount = 0; // Из них прошли проверку уклона.
        int stairTriangleCount = 0; // Треугольники объектов-лестниц.
        int walkableStairTriangleCount = 0; // Проходимая часть лестниц.

        // Добавляет одну вершину и возвращает её индекс для AddTriangle().
        int AddVertex(float x, float y, float z)
        {
            const int index = static_cast<int>(vertices.size() / 3);
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);
            return index;
        }

        // Добавляет один ориентированный треугольник и его тип области.
        void AddTriangle(int a, int b, int c, unsigned char area)
        {
            indices.push_back(a);
            indices.push_back(b);
            indices.push_back(c);
            areas.push_back(area);
        }
    };

    // Полностью готовая сетка одной комнаты либо всего открытого мира.
    struct RoomNavMesh
    {
        dtNavMesh* mesh = nullptr; // Владение Detour-сеткой.
        dtNavMeshQuery* query = nullptr; // Объект поиска ближайших полигонов/пути.
        int polygonCount = 0; // Быстрая диагностика пустой сетки.
        bool floorOnlyFallback = false; // Использован упрощённый пол комнаты.
        std::string status; // Итог построения понятным текстом.
        // Пары точек для отладочной отрисовки рёбер сетки и препятствий.
        std::vector<DirectX::XMFLOAT3> debugMeshEdges;
        std::vector<DirectX::XMFLOAT3> debugObstacleEdges;
        // Дешёвая цифровая подпись статических коллайдеров этой комнаты.
        // Благодаря ей слой замечает появление, удаление и перемещение объектов
        // без обязательного нажатия debug-кнопки Rebuild.
        std::uint64_t geometryFingerprint = 0;
        std::chrono::steady_clock::time_point nextGeometryCheck{};

        // Освобождает C-объекты Detour, которыми владеет эта структура.
        ~RoomNavMesh()
        {
            if (query) dtFreeNavMeshQuery(query);
            if (mesh) dtFreeNavMesh(mesh);
        }
    };

    // Кеш пути конкретного NPC. Он переиспользуется, пока комната, цель и
    // revision команды не изменились.
    struct CachedPath
    {
        Entity room = INVALID_ENTITY; // Сетка, на которой строился путь.
        DirectX::XMFLOAT3 requestedTarget{}; // Исходная мировая цель запроса.
        std::uint32_t revision = 0; // Версия команды/маршрута мозга.
        std::vector<DirectX::XMFLOAT3> points; // Прямые waypoint в мире.
        std::size_t nextPoint = 0; // Индекс следующей ещё не пройденной точки.
        bool buildAttempted = false; // Не повторять провальный запрос каждый кадр.
        std::string status; // Результат последней операции.
        // Диагностика временного объезда динамического препятствия.
        bool dynamicAvoidanceActive = false;
        Entity dynamicObstacle = INVALID_ENTITY;
        DirectX::XMFLOAT3 avoidanceStart{};
        DirectX::XMFLOAT3 avoidancePoint{};
        std::vector<DirectX::XMFLOAT3> dynamicObstacleEdges;
    };

    // Короткая память выбора стороны объезда. Без неё NPC мог бы каждый кадр
    // переключаться «слева/справа» и дрожать перед движущимся ящиком.
    struct DynamicAvoidanceMemory
    {
        Entity obstacle = INVALID_ENTITY; // Какой объект сейчас обходим.
        float side = 1.0f; // -1 = слева, +1 = справа относительно пути.
        std::chrono::steady_clock::time_point keepSideUntil{}; // До какого момента не менять сторону.
    };

    // Все значения ниже — внутренние кеши текущей сцены, а не сериализуемые
    // компоненты. InvalidateAll() полностью очищает их при смене геометрии.
    std::unordered_map<Entity, std::unique_ptr<RoomNavMesh>> g_roomMeshes; // Room Entity -> сетка.
    std::unique_ptr<RoomNavMesh> g_worldMesh; // Общая сетка вне комнат.
    std::unordered_map<Entity, CachedPath> g_paths; // NPC Entity -> текущий путь.
    std::unordered_map<Entity, DynamicAvoidanceMemory> g_dynamicAvoidance; // NPC -> сторона объезда.
    NpcNavMeshDebugDrawSettings g_debugDrawSettings; // Общие флаги визуализации.

    // Превращает AABB в 12 пар точек-линий для debug renderer.
    void AppendDebugBoxEdges(
        std::vector<DirectX::XMFLOAT3>& lines,
        const BuildGeometry::ObstacleBox& box)
    {
        const float x0 = box.minimum[0];
        const float y0 = box.minimum[1];
        const float z0 = box.minimum[2];
        const float x1 = box.maximum[0];
        const float y1 = box.maximum[1];
        const float z1 = box.maximum[2];
        const DirectX::XMFLOAT3 corners[8] = {
            {x0, y0, z0}, {x1, y0, z0}, {x1, y0, z1}, {x0, y0, z1},
            {x0, y1, z0}, {x1, y1, z0}, {x1, y1, z1}, {x0, y1, z1}
        };
        constexpr int edges[12][2] = {
            {0,1}, {1,2}, {2,3}, {3,0}, {4,5}, {5,6},
            {6,7}, {7,4}, {0,4}, {1,5}, {2,6}, {3,7}
        };
        for (const auto& edge : edges)
        {
            lines.push_back(corners[edge[0]]);
            lines.push_back(corners[edge[1]]);
        }
    }

    // Извлекает рёбра Recast-полигонов в простой мировой формат XMFLOAT3.
    void CapturePolyMeshEdges(const rcPolyMesh& mesh, RoomNavMesh& result)
    {
        result.debugMeshEdges.clear();
        for (int polygonIndex = 0; polygonIndex < mesh.npolys; ++polygonIndex)
        {
            const unsigned short* polygon =
                &mesh.polys[polygonIndex * mesh.nvp * 2];
            int vertexCount = 0;
            while (vertexCount < mesh.nvp && polygon[vertexCount] != RC_MESH_NULL_IDX)
                ++vertexCount;

            for (int edgeIndex = 0; edgeIndex < vertexCount; ++edgeIndex)
            {
                const unsigned short a = polygon[edgeIndex];
                const unsigned short b = polygon[(edgeIndex + 1) % vertexCount];

                const unsigned short* va = &mesh.verts[a * 3];
                const unsigned short* vb = &mesh.verts[b * 3];
                result.debugMeshEdges.push_back({
                    mesh.bmin[0] + va[0] * mesh.cs,
                    mesh.bmin[1] + va[1] * mesh.ch,
                    mesh.bmin[2] + va[2] * mesh.cs
                });
                result.debugMeshEdges.push_back({
                    mesh.bmin[0] + vb[0] * mesh.cs,
                    mesh.bmin[1] + vb[1] * mesh.ch,
                    mesh.bmin[2] + vb[2] * mesh.cs
                });
            }
        }
    }

    // Горизонтальная дистанция: высота этажа не влияет на прохождение waypoint.
    float DistanceXZ(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b)
    {
        const float dx = b.x - a.x;
        const float dz = b.z - a.z;
        return std::sqrt(dx * dx + dz * dz);
    }

    // Обычное преобразование локальной точки полной мировой матрицей.
    DirectX::XMFLOAT3 TransformPoint(
        const DirectX::XMFLOAT3& point,
        const DirectX::XMMATRIX& matrix)
    {
        DirectX::XMFLOAT3 result{};
        DirectX::XMStoreFloat3(
            &result,
            DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&point), matrix));
        return result;
    }

    // Добавляет в BuildGeometry прямоугольный участок пола из двух треугольников.
    void AddFloorQuad(
        BuildGeometry& geometry,
        const RoomComponent& room,
        const TransformComponent& transform,
        DirectX::XMFLOAT3& outMin,
        DirectX::XMFLOAT3& outMax)
    {
        // RoomComponent уже содержит локальные bounds доступного отсека. Для
        // Recast создаём гарантированную walkable-поверхность по нижней границе.
        // Статические коллайдеры ниже будут вырезаны из неё как препятствия.
        const float floorY = room.localMin.y + 0.03f;
        const std::array<DirectX::XMFLOAT3, 4> localCorners = {{
            { room.localMin.x, floorY, room.localMin.z },
            { room.localMax.x, floorY, room.localMin.z },
            { room.localMax.x, floorY, room.localMax.z },
            { room.localMin.x, floorY, room.localMax.z }
        }};

        std::array<int, 4> vertexIndices{};
        outMin = {
            (std::numeric_limits<float>::max)(),
            (std::numeric_limits<float>::max)(),
            (std::numeric_limits<float>::max)()
        };
        outMax = {
            -(std::numeric_limits<float>::max)(),
            -(std::numeric_limits<float>::max)(),
            -(std::numeric_limits<float>::max)()
        };

        const DirectX::XMMATRIX world = transform.GetWorldMatrix();
        for (std::size_t i = 0; i < localCorners.size(); ++i)
        {
            const DirectX::XMFLOAT3 p = TransformPoint(localCorners[i], world);
            vertexIndices[i] = geometry.AddVertex(p.x, p.y, p.z);
            outMin.x = (std::min)(outMin.x, p.x);
            outMin.y = (std::min)(outMin.y, p.y);
            outMin.z = (std::min)(outMin.z, p.z);
            outMax.x = (std::max)(outMax.x, p.x);
            outMax.y = (std::max)(outMax.y, p.y);
            outMax.z = (std::max)(outMax.z, p.z);
        }

        // Порядок вершин даёт нормаль вверх для стандартной DirectX-сцены Y-up.
        geometry.AddTriangle(vertexIndices[0], vertexIndices[2], vertexIndices[1], RC_WALKABLE_AREA);
        geometry.AddTriangle(vertexIndices[0], vertexIndices[3], vertexIndices[2], RC_WALKABLE_AREA);
    }

    // Добавляет закрытый AABB препятствия в геометрию и debug-список.
    void AddObstacleBox(
        BuildGeometry& geometry,
        const btVector3& minimum,
        const btVector3& maximum)
    {
        // Recast работает заметно стабильнее, если объёмы препятствий помечаются
        // уже после превращения пола в compact heightfield. Если растеризовать
        // пересекающиеся AABB вместе с полом, они могут стереть весь walkable-слой.
        BuildGeometry::ObstacleBox box;
        box.minimum[0] = minimum.x();
        box.minimum[1] = minimum.y();
        box.minimum[2] = minimum.z();
        box.maximum[0] = maximum.x();
        box.maximum[1] = maximum.y();
        box.maximum[2] = maximum.z();
        geometry.obstacles.push_back(box);
        return;

#if 0
        const int base = static_cast<int>(geometry.vertices.size() / 3);
        const float x0 = minimum.x();
        const float y0 = minimum.y();
        const float z0 = minimum.z();
        const float x1 = maximum.x();
        const float y1 = maximum.y();
        const float z1 = maximum.z();

        const std::array<std::array<float, 3>, 8> points = {{
            {{x0, y0, z0}}, {{x1, y0, z0}}, {{x1, y0, z1}}, {{x0, y0, z1}},
            {{x0, y1, z0}}, {{x1, y1, z0}}, {{x1, y1, z1}}, {{x0, y1, z1}}
        }};
        for (const auto& p : points)
            geometry.AddVertex(p[0], p[1], p[2]);

        constexpr int triangles[12][3] = {
            {0, 1, 2}, {0, 2, 3}, {4, 6, 5}, {4, 7, 6},
            {0, 4, 5}, {0, 5, 1}, {1, 5, 6}, {1, 6, 2},
            {2, 6, 7}, {2, 7, 3}, {3, 7, 4}, {3, 4, 0}
        };
        for (const auto& triangle : triangles)
        {
            // RC_NULL_AREA всё равно создаёт solid span, но никогда не становится
            // проходимым полигоном. Так верх шкафа не превращается в второй этаж.
            geometry.AddTriangle(
                base + triangle[0], base + triangle[1], base + triangle[2], RC_NULL_AREA);
        }
#endif
    }

    // Проверяет, является ли Entity частью корабельной панели, которую надо
    // учитывать как геометрию/препятствие при запекании.
    bool IsShipPanelEntity(ECSRegistry& registry, Entity entity)
    {
        const auto* name = registry.GetComponent<NameComponent>(entity);
        if (!name)
            return false;

        std::string upper = name->name;
        std::transform(
            upper.begin(), upper.end(), upper.begin(),
            [](unsigned char value)
            {
                return static_cast<char>(std::toupper(value));
            });
        return upper.rfind("SHIP_PANEL", 0) == 0;
    }

    // Явная авторская пометка NavMeshSurfaceComponent имеет приоритет над
    // догадками по имени и типу объекта.
    bool IsMarkedWalkableSurface(ECSRegistry& registry, Entity entity)
    {
        const auto* surface = registry.GetComponent<NavMeshSurfaceComponent>(entity);
        return surface && surface->enabled;
    }

    // Определяет лестницу/рамп по компонентам и имени, чтобы разрешить её уклон.
    bool IsStairSurfaceEntity(
        ECSRegistry& registry,
        Entity entity,
        const RigidBodyComponent* rigidBody = nullptr)
    {
        if (rigidBody && rigidBody->shapeType == RigidBodyComponent::StairRamp)
            return true;

        const auto* name = registry.GetComponent<NameComponent>(entity);
        if (!name)
            return false;
        std::string upper = name->name;
        std::transform(
            upper.begin(), upper.end(), upper.begin(),
            [](unsigned char value)
            {
                return static_cast<char>(std::toupper(value));
            });
        return upper.find("STAIR") != std::string::npos ||
            upper.find("BRIDGE") != std::string::npos ||
            upper.find("RAMP") != std::string::npos;
    }

    class WalkableTriangleCollector final : public btTriangleCallback
    {
    public:
        WalkableTriangleCollector(
            BuildGeometry& geometry,
            const btTransform& worldTransform,
            const DirectX::XMFLOAT3& roomMin,
            const DirectX::XMFLOAT3& roomMax,
            bool stairSurface)
            : m_geometry(geometry)
            , m_worldTransform(worldTransform)
            , m_roomMin(roomMin)
            , m_roomMax(roomMax)
            , m_stairSurface(stairSurface)
        {
        }

        void processTriangle(btVector3* triangle, int, int) override
        {
            btVector3 world[3] = {
                m_worldTransform * triangle[0],
                m_worldTransform * triangle[1],
                m_worldTransform * triangle[2]
            };

            const float minX = (std::min)({ world[0].x(), world[1].x(), world[2].x() });
            const float maxX = (std::max)({ world[0].x(), world[1].x(), world[2].x() });
            const float minY = (std::min)({ world[0].y(), world[1].y(), world[2].y() });
            const float maxY = (std::max)({ world[0].y(), world[1].y(), world[2].y() });
            const float minZ = (std::min)({ world[0].z(), world[1].z(), world[2].z() });
            const float maxZ = (std::max)({ world[0].z(), world[1].z(), world[2].z() });
            if (maxX < m_roomMin.x || minX > m_roomMax.x ||
                maxY < m_roomMin.y || minY > m_roomMax.y ||
                maxZ < m_roomMin.z || minZ > m_roomMax.z)
            {
                return;
            }

            btVector3 normal = (world[1] - world[0]).cross(world[2] - world[0]);
            const btScalar lengthSquared = normal.length2();
            if (lengthSquared <= SIMD_EPSILON)
                return;
            normal /= btSqrt(lengthSquared);

            // Та же граница уклона, что и config.walkableSlopeAngle ниже.
            // Верхняя грань пола имеет normal.y > cos(45°). Стены и нижние
            // поверхности всё равно растеризуются, но не получают walkable area.
            const bool walkable = normal.y() > 0.70710678f;
            const int a = m_geometry.AddVertex(world[0].x(), world[0].y(), world[0].z());
            const int b = m_geometry.AddVertex(world[1].x(), world[1].y(), world[1].z());
            const int c = m_geometry.AddVertex(world[2].x(), world[2].y(), world[2].z());
            m_geometry.AddTriangle(
                a, b, c, walkable ? RC_WALKABLE_AREA : RC_NULL_AREA);
            ++m_geometry.surfaceTriangleCount;
            if (walkable)
                ++m_geometry.walkableSurfaceTriangleCount;
            if (m_stairSurface)
            {
                ++m_geometry.stairTriangleCount;
                if (walkable)
                    ++m_geometry.walkableStairTriangleCount;
            }
        }

    private:
        BuildGeometry& m_geometry;
        btTransform m_worldTransform;
        DirectX::XMFLOAT3 m_roomMin{};
        DirectX::XMFLOAT3 m_roomMax{};
        bool m_stairSurface = false;
    };

    // Разбирает Bullet collision shape (включая compound/concave) и добавляет
    // его проходимые мировые треугольники в сырьё Recast.
    void CollectWalkableShapeTriangles(
        BuildGeometry& geometry,
        btCollisionShape* shape,
        const btTransform& worldTransform,
        const DirectX::XMFLOAT3& roomMin,
        const DirectX::XMFLOAT3& roomMax,
        bool stairSurface)
    {
        if (!shape)
            return;

        if (shape->isCompound())
        {
            auto* compound = static_cast<btCompoundShape*>(shape);
            for (int i = 0; i < compound->getNumChildShapes(); ++i)
            {
                CollectWalkableShapeTriangles(
                    geometry,
                    compound->getChildShape(i),
                    worldTransform * compound->getChildTransform(i),
                    roomMin,
                    roomMax,
                    stairSurface);
            }
            return;
        }

        WalkableTriangleCollector collector(
            geometry, worldTransform, roomMin, roomMax, stairSurface);
        if (shape->isConcave())
        {
            auto* concave = static_cast<btConcaveShape*>(shape);
            const btVector3 queryMin(-BT_LARGE_FLOAT, -BT_LARGE_FLOAT, -BT_LARGE_FLOAT);
            const btVector3 queryMax( BT_LARGE_FLOAT,  BT_LARGE_FLOAT,  BT_LARGE_FLOAT);
            concave->processAllTriangles(&collector, queryMin, queryMax);
            return;
        }

        // Editor StairRamp использует convex hull, а импортированная лестница -
        // concave triangle mesh. btShapeHull даёт обоим вариантам один и тот же
        // поток треугольников для Recast.
        if (shape->isConvex())
        {
            auto* convex = static_cast<btConvexShape*>(shape);
            btShapeHull hull(convex);
            if (!hull.buildHull(convex->getMargin()))
                return;
            const btVector3* vertices = hull.getVertexPointer();
            const unsigned int* indices = hull.getIndexPointer();
            for (int i = 0; i + 2 < hull.numIndices(); i += 3)
            {
                btVector3 triangle[3] = {
                    vertices[indices[i]],
                    vertices[indices[i + 1]],
                    vertices[indices[i + 2]]
                };
                collector.processTriangle(triangle, 0, static_cast<int>(i / 3));
            }
        }
    }

    // Собирает одну явно проходимую поверхность из ECS/Bullet.
    void CollectWalkableSurface(
        BuildGeometry& geometry,
        btCollisionObject* body,
        const DirectX::XMFLOAT3& roomMin,
        const DirectX::XMFLOAT3& roomMax,
        bool stairSurface = false)
    {
        if (!body || !body->getCollisionShape())
            return;
        CollectWalkableShapeTriangles(
            geometry,
            body->getCollisionShape(),
            body->getWorldTransform(),
            roomMin,
            roomMax,
            stairSurface);
    }

    // Двери исключаются из постоянной сетки: их доступность меняется во время игры.
    bool IsDoorEntity(ECSRegistry& registry, Entity entity)
    {
        if (registry.HasComponent<DoorComponent>(entity))
            return true;

        for (auto& [doorEntity, door] : registry.GetComponents<DoorComponent>())
        {
            (void)doorEntity;
            if (door.leftLeaf == entity || door.rightLeaf == entity)
                return true;
        }
        return false;
    }

    // Смешивает одно значение в цифровую подпись геометрии.
    void HashCombine(std::uint64_t& seed, std::uint64_t value)
    {
        seed ^= value + 0x9E3779B97F4A7C15ull + (seed << 6) + (seed >> 2);
    }

    // Округляет float перед хешированием: микрошум физики не перестраивает сетку.
    void HashQuantizedFloat(std::uint64_t& seed, btScalar value)
    {
        // Миллиметровой точности для навигации достаточно. Округление не даёт
        // безвредному float-шуму kinematic transform запускать лишний rebuild.
        const auto quantized = static_cast<std::int64_t>(
            std::llround(static_cast<double>(value) * 1000.0));
        HashCombine(seed, static_cast<std::uint64_t>(quantized));
    }

    // Проверяет пересечение мирового AABB Bullet-тела с bounds комнаты.
    bool BodyIntersectsRoom(
        btCollisionObject* body,
        const DirectX::XMFLOAT3& roomMin,
        const DirectX::XMFLOAT3& roomMax,
        btVector3& outMinimum,
        btVector3& outMaximum)
    {
        if (!body || !body->getCollisionShape())
            return false;

        body->getCollisionShape()->getAabb(
            body->getWorldTransform(), outMinimum, outMaximum);
        return !(outMaximum.x() < roomMin.x || outMinimum.x() > roomMax.x ||
            outMaximum.y() < roomMin.y || outMinimum.y() > roomMax.y ||
            outMaximum.z() < roomMin.z || outMinimum.z() > roomMax.z);
    }

    // Добавляет форму и Transform одного значимого Bullet-тела в fingerprint.
    void HashNavigationBody(
        std::uint64_t& signature,
        Entity entity,
        btCollisionObject* body,
        const DirectX::XMFLOAT3& roomMin,
        const DirectX::XMFLOAT3& roomMax)
    {
        btVector3 minimum{};
        btVector3 maximum{};
        if (!BodyIntersectsRoom(body, roomMin, roomMax, minimum, maximum))
            return;

        HashCombine(signature, static_cast<std::uint64_t>(entity));
        HashCombine(signature, reinterpret_cast<std::uintptr_t>(body));
        HashCombine(signature, reinterpret_cast<std::uintptr_t>(body->getCollisionShape()));
        HashQuantizedFloat(signature, minimum.x());
        HashQuantizedFloat(signature, minimum.y());
        HashQuantizedFloat(signature, minimum.z());
        HashQuantizedFloat(signature, maximum.x());
        HashQuantizedFloat(signature, maximum.y());
        HashQuantizedFloat(signature, maximum.z());
    }

    // Подпись всей навигационной геометрии комнаты для автоматической
    // инвалидизации кеша после перемещения/добавления препятствия.
    std::uint64_t CalculateRoomGeometryFingerprint(
        ECSRegistry& registry,
        Entity roomEntity)
    {
        const auto* room = registry.GetComponent<RoomComponent>(roomEntity);
        const auto* roomTransform = registry.GetComponent<TransformComponent>(roomEntity);
        if (!room || !roomTransform)
            return 0;

        BuildGeometry boundsGeometry;
        DirectX::XMFLOAT3 roomMin{};
        DirectX::XMFLOAT3 roomMax{};
        AddFloorQuad(boundsGeometry, *room, *roomTransform, roomMin, roomMax);
        roomMin.y -= 2.0f;
        roomMax.y = (std::max)(roomMax.y + 4.0f, roomMin.y + 6.0f);

        std::uint64_t signature = 1469598103934665603ull;
        HashCombine(signature, static_cast<std::uint64_t>(roomEntity));
        HashQuantizedFloat(signature, roomMin.x);
        HashQuantizedFloat(signature, roomMin.y);
        HashQuantizedFloat(signature, roomMin.z);
        HashQuantizedFloat(signature, roomMax.x);
        HashQuantizedFloat(signature, roomMax.y);
        HashQuantizedFloat(signature, roomMax.z);

        std::unordered_set<const btCollisionObject*> addedBodies;
        for (auto& [entity, rigidBody] : registry.GetComponents<RigidBodyComponent>())
        {
            if (!rigidBody.body || !rigidBody.body->isStaticOrKinematicObject() ||
                entity == roomEntity || IsDoorEntity(registry, entity) ||
                registry.HasComponent<CharacterController>(entity))
            {
                continue;
            }
            if (addedBodies.insert(rigidBody.body).second)
            {
                HashNavigationBody(
                    signature, entity, rigidBody.body, roomMin, roomMax);
            }
        }

        for (auto& [entity, collider] : registry.GetComponents<StaticColliderComponent>())
        {
            if (!collider.body || entity == roomEntity || IsDoorEntity(registry, entity))
                continue;
            if (addedBodies.insert(collider.body).second)
            {
                HashNavigationBody(
                    signature, entity, collider.body, roomMin, roomMax);
            }
        }
        for (auto& [entity, surface] : registry.GetComponents<NavMeshSurfaceComponent>())
        {
            HashCombine(signature, static_cast<std::uint64_t>(entity));
            HashCombine(signature, surface.enabled ? 1 : 0);
        }
        return signature;
    }

    btCollisionObject* GetWorldSurfaceBody(ECSRegistry& registry, Entity entity)
    {
        if (!registry.IsAlive(entity) || IsDoorEntity(registry, entity) ||
            registry.HasComponent<CharacterController>(entity))
            return nullptr;
        if (auto* rigidBody = registry.GetComponent<RigidBodyComponent>(entity))
        {
            // Bullet's mathematical Plane has an unbounded AABB and cannot be
            // rasterized into a finite scene mesh. Use a thin Box for ground.
            if (rigidBody->shapeType != RigidBodyComponent::Plane &&
                rigidBody->body && rigidBody->body->isStaticOrKinematicObject() &&
                rigidBody->body->getCollisionShape())
                return rigidBody->body;
        }
        if (auto* collider = registry.GetComponent<StaticColliderComponent>(entity))
        {
            if (collider->body && collider->body->getCollisionShape())
                return collider->body;
        }
        return nullptr;
    }

    // Находит общие мировые границы явно отмеченных навигационных поверхностей.
    bool GetWorldSurfaceBounds(
        ECSRegistry& registry,
        DirectX::XMFLOAT3& outMin,
        DirectX::XMFLOAT3& outMax)
    {
        bool found = false;
        for (auto& [entity, surface] : registry.GetComponents<NavMeshSurfaceComponent>())
        {
            if (!surface.enabled)
                continue;
            btCollisionObject* body = GetWorldSurfaceBody(registry, entity);
            if (!body)
                continue;
            btVector3 minimum{}, maximum{};
            body->getCollisionShape()->getAabb(body->getWorldTransform(), minimum, maximum);
            if (!std::isfinite(minimum.x()) || !std::isfinite(minimum.y()) ||
                !std::isfinite(minimum.z()) || !std::isfinite(maximum.x()) ||
                !std::isfinite(maximum.y()) || !std::isfinite(maximum.z()) ||
                maximum.x() - minimum.x() > 10000.0f ||
                maximum.z() - minimum.z() > 10000.0f)
                continue;
            if (!found)
            {
                outMin = { minimum.x(), minimum.y(), minimum.z() };
                outMax = { maximum.x(), maximum.y(), maximum.z() };
                found = true;
            }
            else
            {
                outMin.x = (std::min)(outMin.x, minimum.x());
                outMin.y = (std::min)(outMin.y, minimum.y());
                outMin.z = (std::min)(outMin.z, minimum.z());
                outMax.x = (std::max)(outMax.x, maximum.x());
                outMax.y = (std::max)(outMax.y, maximum.y());
                outMax.z = (std::max)(outMax.z, maximum.z());
            }
        }
        if (found)
        {
            outMin.y -= 2.0f;
            outMax.y += 4.0f;
        }
        return found;
    }

    // То же, что fingerprint комнаты, но для общей открытой сетки сцены.
    std::uint64_t CalculateWorldGeometryFingerprint(ECSRegistry& registry)
    {
        std::uint64_t signature = 1469598103934665603ull;
        DirectX::XMFLOAT3 boundsMin{}, boundsMax{};
        const bool hasSurfaces = GetWorldSurfaceBounds(registry, boundsMin, boundsMax);
        HashCombine(signature, hasSurfaces ? 1 : 0);
        for (auto& [entity, surface] : registry.GetComponents<NavMeshSurfaceComponent>())
        {
            HashCombine(signature, static_cast<std::uint64_t>(entity));
            HashCombine(signature, surface.enabled ? 1 : 0);
            btCollisionObject* body = GetWorldSurfaceBody(registry, entity);
            HashCombine(signature, reinterpret_cast<std::uintptr_t>(body));
        }
        if (!hasSurfaces)
            return signature;

        std::unordered_set<const btCollisionObject*> addedBodies;
        for (auto& [entity, rigidBody] : registry.GetComponents<RigidBodyComponent>())
        {
            if (!rigidBody.body || !rigidBody.body->isStaticOrKinematicObject() ||
                IsDoorEntity(registry, entity) ||
                registry.HasComponent<CharacterController>(entity))
                continue;
            if (addedBodies.insert(rigidBody.body).second)
                HashNavigationBody(signature, entity, rigidBody.body, boundsMin, boundsMax);
        }
        for (auto& [entity, collider] : registry.GetComponents<StaticColliderComponent>())
        {
            if (!collider.body || IsDoorEntity(registry, entity) ||
                registry.HasComponent<CharacterController>(entity))
                continue;
            if (addedBodies.insert(collider.body).second)
                HashNavigationBody(signature, entity, collider.body, boundsMin, boundsMax);
        }
        return signature;
    }

    // Добавляет одно неподвижное/кинематическое тело как вырезаемое препятствие.
    void CollectObstacle(
        BuildGeometry& geometry,
        btCollisionObject* body,
        const DirectX::XMFLOAT3& roomMin,
        const DirectX::XMFLOAT3& roomMax)
    {
        if (!body || !body->getCollisionShape())
            return;

        btVector3 obstacleMin{};
        btVector3 obstacleMax{};
        body->getCollisionShape()->getAabb(body->getWorldTransform(), obstacleMin, obstacleMax);

        // Не добавляем коллайдеры, которые не пересекают горизонтальную область
        // комнаты или полностью находятся под её полом.
        if (obstacleMax.x() < roomMin.x || obstacleMin.x() > roomMax.x ||
            obstacleMax.z() < roomMin.z || obstacleMin.z() > roomMax.z ||
            obstacleMax.y() < roomMin.y + 0.08f)
        {
            return;
        }

        const float roomWidth = (std::max)(0.1f, roomMax.x - roomMin.x);
        const float roomDepth = (std::max)(0.1f, roomMax.z - roomMin.z);
        const float obstacleWidth = obstacleMax.x() - obstacleMin.x();
        const float obstacleDepth = obstacleMax.z() - obstacleMin.z();

        // Большой hull корпуса/пола часто окружает целую комнату одним AABB.
        // Если добавить такой AABB как препятствие, он сотрёт весь NavMesh.
        // Стены уже задаются границей floor quad, поэтому этот conservative box
        // безопаснее пропустить. Мебель и локальные препятствия остаются.
        if (obstacleWidth >= roomWidth * 0.90f && obstacleDepth >= roomDepth * 0.90f)
            return;

        btVector3 clippedMin(
            (std::max)(obstacleMin.x(), roomMin.x),
            (std::max)(obstacleMin.y(), roomMin.y - 0.5f),
            (std::max)(obstacleMin.z(), roomMin.z));
        btVector3 clippedMax(
            (std::min)(obstacleMax.x(), roomMax.x),
            (std::min)(obstacleMax.y(), roomMax.y + 3.0f),
            (std::min)(obstacleMax.z(), roomMax.z));

        if (clippedMax.x() - clippedMin.x() < 0.05f ||
            clippedMax.z() - clippedMin.z() < 0.05f)
        {
            return;
        }
        AddObstacleBox(geometry, clippedMin, clippedMax);
    }

    // Собирает полы, лестницы и препятствия внутри заданной комнаты.
    void CollectRoomGeometry(
        ECSRegistry& registry,
        Entity roomEntity,
        BuildGeometry& geometry,
        DirectX::XMFLOAT3& roomMin,
        DirectX::XMFLOAT3& roomMax,
        bool useShipPanelSurface)
    {
        const auto* room = registry.GetComponent<RoomComponent>(roomEntity);
        const auto* transform = registry.GetComponent<TransformComponent>(roomEntity);
        if (!room || !transform)
            return;

        AddFloorQuad(geometry, *room, *transform, roomMin, roomMax);
        if (useShipPanelSurface)
        {
            // AddFloorQuad здесь использовался только для вычисления мировых
            // границ SHIP_ROOM. В основной попытке искусственную плоскость сразу
            // удаляем: поверхность дальше придёт из реальных SHIP_PANEL triangles.
            geometry.vertices.clear();
            geometry.indices.clear();
            geometry.areas.clear();
        }

        // Углубление пола может находиться ниже исходного room.localMin. Два
        // метра запаса позволяют захватить его треугольники, не расширяя область
        // по X/Z в соседние комнаты.
        roomMin.y -= 2.0f;
        roomMax.y = (std::max)(roomMax.y + 4.0f, roomMin.y + 6.0f);

        std::unordered_set<const btCollisionObject*> addedBodies;
        for (auto& [entity, rigidBody] : registry.GetComponents<RigidBodyComponent>())
        {
            if (!rigidBody.body || !rigidBody.body->isStaticOrKinematicObject() ||
                entity == roomEntity || IsDoorEntity(registry, entity) ||
                registry.HasComponent<CharacterController>(entity))
            {
                continue;
            }
            if (addedBodies.insert(rigidBody.body).second)
            {
                const bool stairSurface =
                    IsStairSurfaceEntity(registry, entity, &rigidBody);
                if (IsShipPanelEntity(registry, entity) || stairSurface ||
                    IsMarkedWalkableSurface(registry, entity))
                {
                    if (useShipPanelSurface)
                        CollectWalkableSurface(
                            geometry, rigidBody.body, roomMin, roomMax, stairSurface);
                }
                else
                {
                    CollectObstacle(geometry, rigidBody.body, roomMin, roomMax);
                }
            }
        }

        for (auto& [entity, collider] : registry.GetComponents<StaticColliderComponent>())
        {
            if (!collider.body || entity == roomEntity || IsDoorEntity(registry, entity))
                continue;
            if (addedBodies.insert(collider.body).second)
            {
                const bool stairSurface = IsStairSurfaceEntity(registry, entity);
                if (IsShipPanelEntity(registry, entity) || stairSurface ||
                    IsMarkedWalkableSurface(registry, entity))
                {
                    if (useShipPanelSurface)
                        CollectWalkableSurface(
                            geometry, collider.body, roomMin, roomMax, stairSurface);
                }
                else
                {
                    CollectObstacle(geometry, collider.body, roomMin, roomMax);
                }
            }
        }
    }

    // Собирает геометрию открытого мира без RoomComponent.
    bool CollectWorldGeometry(
        ECSRegistry& registry,
        BuildGeometry& geometry,
        DirectX::XMFLOAT3& boundsMin,
        DirectX::XMFLOAT3& boundsMax)
    {
        if (!GetWorldSurfaceBounds(registry, boundsMin, boundsMax))
            return false;

        std::unordered_set<const btCollisionObject*> addedBodies;
        for (auto& [entity, surface] : registry.GetComponents<NavMeshSurfaceComponent>())
        {
            if (!surface.enabled)
                continue;
            btCollisionObject* body = GetWorldSurfaceBody(registry, entity);
            if (body && addedBodies.insert(body).second)
                CollectWalkableSurface(geometry, body, boundsMin, boundsMax);
        }

        // Unmarked static colliders are obstacles, never extra walkable islands.
        for (auto& [entity, rigidBody] : registry.GetComponents<RigidBodyComponent>())
        {
            if (!rigidBody.body || !rigidBody.body->isStaticOrKinematicObject() ||
                IsDoorEntity(registry, entity) ||
                registry.HasComponent<CharacterController>(entity))
                continue;
            if (addedBodies.insert(rigidBody.body).second)
                CollectObstacle(geometry, rigidBody.body, boundsMin, boundsMax);
        }
        for (auto& [entity, collider] : registry.GetComponents<StaticColliderComponent>())
        {
            if (!collider.body || IsDoorEntity(registry, entity) ||
                registry.HasComponent<CharacterController>(entity))
                continue;
            if (addedBodies.insert(collider.body).second)
                CollectObstacle(geometry, collider.body, boundsMin, boundsMax);
        }
        return true;
    }

    // Один конкретный запуск конвейера Recast с переданными настройками.
    // Возвращает nullptr и текст причины, если любой этап не удался.
    std::unique_ptr<RoomNavMesh> BuildRoomNavMeshAttempt(
        ECSRegistry& registry,
        Entity roomEntity,
        bool includeObstacles,
        bool useShipPanelSurface,
        bool worldSurfaces = false)
    {
        auto result = std::make_unique<RoomNavMesh>();
        BuildGeometry geometry;
        DirectX::XMFLOAT3 roomMin{};
        DirectX::XMFLOAT3 roomMax{};
        if (worldSurfaces)
            CollectWorldGeometry(registry, geometry, roomMin, roomMax);
        else
            CollectRoomGeometry(
                registry, roomEntity, geometry, roomMin, roomMax, useShipPanelSurface);

        // AABB сохраняются даже если полноценная сборка ниже провалится. Именно
        // эти красные коробки помогут глазами найти коллайдер, который случайно
        // перекрыл весь пол комнаты.
        for (const BuildGeometry::ObstacleBox& obstacle : geometry.obstacles)
            AppendDebugBoxEdges(result->debugObstacleEdges, obstacle);

        // Во второй попытке оставляем только пол. Это страховка от ошибочного
        // физического AABB, который случайно накрывает всю комнату. Такой режим
        // явно подписывается в debug-статусе и не маскируется под обычный успех.
        if (!includeObstacles)
            geometry.obstacles.clear();

        const int vertexCount = static_cast<int>(geometry.vertices.size() / 3);
        const int triangleCount = static_cast<int>(geometry.indices.size() / 3);
        if (vertexCount < 3 || triangleCount < 1)
        {
            result->status = worldSurfaces
                ? "NavMesh build skipped: add NavMesh Surface to static walkable collision"
                : useShipPanelSurface
                    ? "NavMesh build skipped: no walkable collision triangles intersect room"
                    : "NavMesh build skipped: room has no usable fallback geometry";
            return result;
        }

        rcContext context(false);
        rcConfig config{};
        config.cs = 0.20f;
        config.ch = 0.10f;
        config.walkableSlopeAngle = 45.0f;
        config.walkableHeight = static_cast<int>(std::ceil(1.80f / config.ch));
        // Ограничиваем маршруты возможностями физического контроллера-капсулы.
        // Раньше Recast разрешал ступени высотой 45 см, хотя контроллер безопасно
        // переступал только более низкие: NPC строил правильный на бумаге маршрут,
        // который затем физически не мог пройти.
        constexpr float physicalWalkableClimb = 0.30f;
        config.walkableClimb = static_cast<int>(
            std::floor(physicalWalkableClimb / config.ch));
        config.walkableRadius = static_cast<int>(std::ceil(0.42f / config.cs));
        config.maxEdgeLen = static_cast<int>(12.0f / config.cs);
        config.maxSimplificationError = 1.3f;
        config.minRegionArea = rcSqr(4);
        config.mergeRegionArea = rcSqr(12);
        config.maxVertsPerPoly = 6;
        config.detailSampleDist = config.cs * 6.0f;
        config.detailSampleMaxError = config.ch * 1.0f;
        config.bmin[0] = roomMin.x - 0.5f;
        config.bmin[1] = roomMin.y - 0.5f;
        config.bmin[2] = roomMin.z - 0.5f;
        config.bmax[0] = roomMax.x + 0.5f;
        config.bmax[1] = roomMax.y + 0.5f;
        config.bmax[2] = roomMax.z + 0.5f;
        if (config.bmax[0] - config.bmin[0] > 10000.0f ||
            config.bmax[2] - config.bmin[2] > 10000.0f)
        {
            result->status = "NavMesh bounds exceed single-mesh limit; split the level into smaller areas";
            return result;
        }
        rcCalcGridSize(config.bmin, config.bmax, config.cs, &config.width, &config.height);
        // A single Recast heightfield is intentionally bounded. Very large
        // worlds will need tiled streaming rather than an unbounded allocation.
        constexpr std::int64_t maxGridCells = 4'000'000;
        if (config.width <= 0 || config.height <= 0 ||
            static_cast<std::int64_t>(config.width) * config.height > maxGridCells)
        {
            result->status = "NavMesh bounds exceed single-mesh limit; split the level into smaller areas";
            return result;
        }

        rcHeightfield* solid = rcAllocHeightfield();
        rcCompactHeightfield* compact = nullptr;
        rcContourSet* contours = nullptr;
        rcPolyMesh* polyMesh = nullptr;
        rcPolyMeshDetail* detailMesh = nullptr;

        auto cleanup = [&]()
        {
            if (solid) rcFreeHeightField(solid);
            if (compact) rcFreeCompactHeightfield(compact);
            if (contours) rcFreeContourSet(contours);
            if (polyMesh) rcFreePolyMesh(polyMesh);
            if (detailMesh) rcFreePolyMeshDetail(detailMesh);
        };

        if (!solid || !rcCreateHeightfield(
                &context, *solid, config.width, config.height,
                config.bmin, config.bmax, config.cs, config.ch) ||
            !rcRasterizeTriangles(
                &context,
                geometry.vertices.data(), vertexCount,
                geometry.indices.data(), geometry.areas.data(), triangleCount,
                *solid, config.walkableClimb))
        {
            result->status = "Recast failed while rasterizing room geometry";
            cleanup();
            return result;
        }

        rcFilterLowHangingWalkableObstacles(&context, config.walkableClimb, *solid);
        rcFilterLedgeSpans(&context, config.walkableHeight, config.walkableClimb, *solid);
        rcFilterWalkableLowHeightSpans(&context, config.walkableHeight, *solid);

        compact = rcAllocCompactHeightfield();
        if (!compact || !rcBuildCompactHeightfield(
                &context, config.walkableHeight, config.walkableClimb, *solid, *compact))
        {
            result->status = "Recast failed while creating compact room heightfield";
            cleanup();
            return result;
        }

        // Сначала строим гарантированно проходимый пол. И только потом удаляем
        // ячейки, занятые статическими коллайдерами. Это штатная схема Recast
        // для коробочных препятствий: пересекающиеся AABB больше не могут
        // испортить первоначальную растеризацию самого пола.
        for (const BuildGeometry::ObstacleBox& obstacle : geometry.obstacles)
        {
            rcMarkBoxArea(
                &context,
                obstacle.minimum,
                obstacle.maximum,
                RC_NULL_AREA,
                *compact);
        }

        if (!rcErodeWalkableArea(&context, config.walkableRadius, *compact) ||
            !rcBuildDistanceField(&context, *compact) ||
            !rcBuildRegions(
                &context, *compact, 0, config.minRegionArea, config.mergeRegionArea))
        {
            result->status = "Recast failed while filtering walkable room area";
            cleanup();
            return result;
        }

        contours = rcAllocContourSet();
        polyMesh = rcAllocPolyMesh();
        detailMesh = rcAllocPolyMeshDetail();
        if (!contours || !polyMesh || !detailMesh ||
            !rcBuildContours(
                &context, *compact, config.maxSimplificationError,
                config.maxEdgeLen, *contours) ||
            !rcBuildPolyMesh(&context, *contours, config.maxVertsPerPoly, *polyMesh) ||
            !rcBuildPolyMeshDetail(
                &context, *polyMesh, *compact,
                config.detailSampleDist, config.detailSampleMaxError, *detailMesh))
        {
            result->status = "Recast failed while creating room polygons";
            cleanup();
            return result;
        }

        // rcBuildPolyMesh() может вернуть true даже при пустом результате.
        // Detour явно запрещает ноль вершин или полигонов, поэтому показываем
        // настоящую причину вместо невнятного общего сообщения Detour failed.
        if (polyMesh->nverts <= 0 || polyMesh->npolys <= 0)
        {
            result->status =
                "Recast produced empty room mesh (verts=" +
                std::to_string(polyMesh->nverts) +
                ", polys=" + std::to_string(polyMesh->npolys) +
                ", surfaceTris=" + std::to_string(geometry.surfaceTriangleCount) +
                ", walkableSurfaceTris=" +
                std::to_string(geometry.walkableSurfaceTriangleCount) +
                ", obstacles=" + std::to_string(geometry.obstacles.size()) + ")";
            cleanup();
            return result;
        }

        CapturePolyMeshEdges(*polyMesh, *result);

        for (int i = 0; i < polyMesh->npolys; ++i)
        {
            if (polyMesh->areas[i] == RC_WALKABLE_AREA)
                polyMesh->areas[i] = NAV_AREA_GROUND;
            polyMesh->flags[i] = NAV_FLAG_WALK;
        }

        dtNavMeshCreateParams params{};
        params.verts = polyMesh->verts;
        params.vertCount = polyMesh->nverts;
        params.polys = polyMesh->polys;
        params.polyAreas = polyMesh->areas;
        params.polyFlags = polyMesh->flags;
        params.polyCount = polyMesh->npolys;
        params.nvp = polyMesh->nvp;
        params.detailMeshes = detailMesh->meshes;
        params.detailVerts = detailMesh->verts;
        params.detailVertsCount = detailMesh->nverts;
        params.detailTris = detailMesh->tris;
        params.detailTriCount = detailMesh->ntris;
        params.walkableHeight = 1.80f;
        params.walkableRadius = 0.42f;
        params.walkableClimb = physicalWalkableClimb;
        rcVcopy(params.bmin, polyMesh->bmin);
        rcVcopy(params.bmax, polyMesh->bmax);
        params.cs = config.cs;
        params.ch = config.ch;
        params.buildBvTree = true;

        unsigned char* navData = nullptr;
        int navDataSize = 0;
        if (!dtCreateNavMeshData(&params, &navData, &navDataSize))
        {
            result->status =
                "Detour rejected room mesh (verts=" +
                std::to_string(polyMesh->nverts) +
                ", polys=" + std::to_string(polyMesh->npolys) +
                ", nvp=" + std::to_string(polyMesh->nvp) +
                ", detailTris=" + std::to_string(detailMesh->ntris) + ")";
            cleanup();
            return result;
        }

        result->mesh = dtAllocNavMesh();
        if (!result->mesh)
        {
            dtFree(navData);
            result->status = "Detour could not allocate room NavMesh";
            cleanup();
            return result;
        }

        const dtStatus meshStatus =
            result->mesh->init(navData, navDataSize, DT_TILE_FREE_DATA);
        if (dtStatusFailed(meshStatus))
        {
            dtFree(navData);
            result->status = "Detour failed to initialize room NavMesh";
            cleanup();
            return result;
        }

        result->query = dtAllocNavMeshQuery();
        if (!result->query || dtStatusFailed(result->query->init(result->mesh, 2048)))
        {
            result->status = "Detour failed to initialize room NavMesh query";
            cleanup();
            return result;
        }

        result->polygonCount = polyMesh->npolys;
        result->floorOnlyFallback = !useShipPanelSurface && !worldSurfaces;
        result->status = worldSurfaces
            ? "World NavMesh ready from marked collision surfaces (triangles=" +
                std::to_string(geometry.surfaceTriangleCount) +
                ", walkable=" + std::to_string(geometry.walkableSurfaceTriangleCount) + ")"
            : useShipPanelSurface
            ? "Room NavMesh ready from walkable collision (triangles=" +
                std::to_string(geometry.surfaceTriangleCount) +
                ", walkable=" +
                std::to_string(geometry.walkableSurfaceTriangleCount) +
                ", stairs=" + std::to_string(geometry.stairTriangleCount) +
                ", walkableStairs=" +
                std::to_string(geometry.walkableStairTriangleCount) + ")"
            : "Room NavMesh ready (flat SHIP_ROOM fallback)";
        cleanup();
        return result;
    }

    // Высокоуровневая сборка: пробует полную геометрию, затем безопасный
    // floor-only fallback, чтобы простая комната всё равно оставалась проходимой.
    std::unique_ptr<RoomNavMesh> BuildRoomNavMesh(ECSRegistry& registry, Entity roomEntity)
    {
        // Сначала пытаемся построить полноценную сетку с препятствиями. Если
        // геометрия сцены плохая, повторяем сборку только с полом: управление не
        // откатывается к слепому прямому движению, а UI сообщает, что надо искать
        // проблемный AABB коллайдера.
        auto primary = BuildRoomNavMeshAttempt(registry, roomEntity, true, true);
        if (primary->mesh && primary->query)
            return primary;

        const std::string primaryFailure = primary->status;
        auto fallback = BuildRoomNavMeshAttempt(registry, roomEntity, false, false);
        if (fallback->mesh && fallback->query)
        {
            // У fallback нет собственных препятствий, поэтому переносим красные
            // diagnostic AABB из провалившейся полноценной попытки.
            fallback->debugObstacleEdges = std::move(primary->debugObstacleEdges);
            fallback->status += "; primary: " + primaryFailure;
            return fallback;
        }

        fallback->status =
            "NavMesh failed with obstacles: " + primaryFailure +
            "; floor-only: " + fallback->status;
        return fallback;
    }

    // Удаляет пути всех NPC, рассчитанные на перестроенной сетке комнаты.
    void ErasePathsForRoom(Entity roomEntity)
    {
        for (auto pathIt = g_paths.begin(); pathIt != g_paths.end();)
        {
            if (pathIt->second.room == roomEntity)
                pathIt = g_paths.erase(pathIt);
            else
                ++pathIt;
        }
    }

    RoomNavMesh* GetOrBuildRoom(ECSRegistry& registry, Entity roomEntity)
    {
        if (roomEntity == INVALID_ENTITY)
            return nullptr;
        auto existing = g_roomMeshes.find(roomEntity);
        if (existing != g_roomMeshes.end())
        {
            RoomNavMesh* cached = existing->second.get();
            const auto now = std::chrono::steady_clock::now();
            if (cached && now >= cached->nextGeometryCheck)
            {
                // Сравнение подписей намного дешевле полной сборки Recast, но и
                // его нет смысла повторять для каждого NPC в каждом кадре.
                cached->nextGeometryCheck = now + std::chrono::milliseconds(750);
                const std::uint64_t currentFingerprint =
                    CalculateRoomGeometryFingerprint(registry, roomEntity);
                if (currentFingerprint != cached->geometryFingerprint)
                {
                    ErasePathsForRoom(roomEntity);
                    auto rebuilt = BuildRoomNavMesh(registry, roomEntity);
                    rebuilt->geometryFingerprint = currentFingerprint;
                    rebuilt->nextGeometryCheck =
                        now + std::chrono::milliseconds(750);
                    rebuilt->status += "; auto-refreshed after static collision change";
                    cached = rebuilt.get();
                    existing->second = std::move(rebuilt);
                    OutputDebugStringA((
                        "[NPC NavMesh] Automatically rebuilt room " +
                        std::to_string(roomEntity) + " after collision change.\n").c_str());
                }
            }
            return cached;
        }

        auto mesh = BuildRoomNavMesh(registry, roomEntity);
        mesh->geometryFingerprint = CalculateRoomGeometryFingerprint(registry, roomEntity);
        mesh->nextGeometryCheck =
            std::chrono::steady_clock::now() + std::chrono::milliseconds(750);
        RoomNavMesh* result = mesh.get();
        g_roomMeshes.emplace(roomEntity, std::move(mesh));
        OutputDebugStringA(("[NPC NavMesh] " + result->status +
            " for room " + std::to_string(roomEntity) + "\n").c_str());
        return result;
    }

    RoomNavMesh* GetOrBuildWorld(ECSRegistry& registry)
    {
        const auto now = std::chrono::steady_clock::now();
        if (g_worldMesh && now < g_worldMesh->nextGeometryCheck)
            return g_worldMesh.get();

        const std::uint64_t fingerprint = CalculateWorldGeometryFingerprint(registry);
        if (g_worldMesh && g_worldMesh->geometryFingerprint == fingerprint)
        {
            g_worldMesh->nextGeometryCheck = now + std::chrono::milliseconds(750);
            return g_worldMesh.get();
        }

        ErasePathsForRoom(INVALID_ENTITY);
        auto rebuilt = BuildRoomNavMeshAttempt(
            registry, INVALID_ENTITY, true, true, true);
        rebuilt->geometryFingerprint = fingerprint;
        rebuilt->nextGeometryCheck = now + std::chrono::milliseconds(750);
        g_worldMesh = std::move(rebuilt);
        OutputDebugStringA(("[NPC NavMesh] " + g_worldMesh->status + "\n").c_str());
        return g_worldMesh.get();
    }

    RoomNavMesh* GetOrBuildNavigationMesh(ECSRegistry& registry, Entity roomEntity)
    {
        return roomEntity == INVALID_ENTITY
            ? GetOrBuildWorld(registry)
            : GetOrBuildRoom(registry, roomEntity);
    }

    // Строит и кеширует путь одного NPC: ближайшие полигоны -> коридор Detour ->
    // набор прямых мировых waypoint. true означает, что есть следующая точка.
    bool BuildPath(
        RoomNavMesh& room,
        const DirectX::XMFLOAT3& start,
        const DirectX::XMFLOAT3& target,
        CachedPath& cache)
    {
        if (!room.mesh || !room.query)
        {
            cache.status = room.status.empty() ? "Room NavMesh unavailable" : room.status;
            return false;
        }

        const float startPosition[3] = { start.x, start.y, start.z };
        const float targetPosition[3] = { target.x, target.y, target.z };
        // Start may sit just outside an eroded polygon while the capsule crosses
        // a room boundary. Target can be an approach point close to that same
        // boundary. Use separate generous extents instead of dropping straight
        // into blind direct movement when either point misses by a few cells.
        const float startSearchExtents[3] = { 2.25f, 3.5f, 2.25f };
        const float targetSearchExtents[3] = { 3.0f, 3.5f, 3.0f };
        dtQueryFilter filter;
        filter.setIncludeFlags(NAV_FLAG_WALK);
        filter.setExcludeFlags(0);

        dtPolyRef startRef = 0;
        dtPolyRef targetRef = 0;
        float nearestStart[3]{};
        float nearestTarget[3]{};
        const dtStatus startProjection = room.query->findNearestPoly(
            startPosition, startSearchExtents, &filter, &startRef, nearestStart);
        const dtStatus targetProjection = room.query->findNearestPoly(
            targetPosition, targetSearchExtents, &filter, &targetRef, nearestTarget);
        if (dtStatusFailed(startProjection) || !startRef ||
            dtStatusFailed(targetProjection) || !targetRef)
        {
            cache.status = "Detour projection failed:";
            if (dtStatusFailed(startProjection) || !startRef)
                cache.status += " start";
            if (dtStatusFailed(targetProjection) || !targetRef)
                cache.status += " target";
            return false;
        }

        dtPolyRef polygonPath[MAX_POLY_PATH]{};
        int polygonPathCount = 0;
        if (dtStatusFailed(room.query->findPath(
                startRef, targetRef, nearestStart, nearestTarget,
                &filter, polygonPath, &polygonPathCount, MAX_POLY_PATH)) ||
            polygonPathCount <= 0)
        {
            cache.status = "Detour could not find a polygon path";
            return false;
        }

        float pathEnd[3] = { nearestTarget[0], nearestTarget[1], nearestTarget[2] };
        if (polygonPath[polygonPathCount - 1] != targetRef)
        {
            room.query->closestPointOnPoly(
                polygonPath[polygonPathCount - 1], nearestTarget, pathEnd, nullptr);
        }

        float straightPoints[MAX_STRAIGHT_PATH * 3]{};
        unsigned char straightFlags[MAX_STRAIGHT_PATH]{};
        dtPolyRef straightRefs[MAX_STRAIGHT_PATH]{};
        int straightPointCount = 0;
        if (dtStatusFailed(room.query->findStraightPath(
                nearestStart, pathEnd, polygonPath, polygonPathCount,
                straightPoints, straightFlags, straightRefs,
                &straightPointCount, MAX_STRAIGHT_PATH)) ||
            straightPointCount <= 0)
        {
            cache.status = "Detour could not create a straight waypoint path";
            return false;
        }

        cache.points.clear();
        cache.points.reserve(straightPointCount);
        for (int i = 0; i < straightPointCount; ++i)
        {
            cache.points.push_back({
                straightPoints[i * 3 + 0],
                straightPoints[i * 3 + 1],
                straightPoints[i * 3 + 2]
            });
        }
        cache.nextPoint = cache.points.size() > 1 ? 1 : 0;
        if (room.floorOnlyFallback)
        {
            cache.status =
                "WARNING: Detour path uses floor-only fallback; obstacles are NOT avoided";
        }
        else
        {
            cache.status = polygonPath[polygonPathCount - 1] == targetRef
                ? "Detour path ready"
                : "Detour returned a partial path to the nearest reachable polygon";
        }
        cache.status += "; " + room.status;
        return true;
    }

    // Проецирует desiredPoint только на связный остров, достижимый от start.
    bool FindConnectedProjectedPoint(
        RoomNavMesh& room,
        const DirectX::XMFLOAT3& start,
        const DirectX::XMFLOAT3& desiredPoint,
        DirectX::XMFLOAT3& outPoint)
    {
        if (!room.mesh || !room.query)
            return false;

        const float startPosition[3] = { start.x, start.y, start.z };
        const float desiredPosition[3] = {
            desiredPoint.x, desiredPoint.y, desiredPoint.z
        };
        const float searchExtents[3] = { 1.5f, 3.0f, 1.5f };
        dtQueryFilter filter;
        filter.setIncludeFlags(NAV_FLAG_WALK);
        filter.setExcludeFlags(0);

        dtPolyRef startRef = 0;
        dtPolyRef targetRef = 0;
        float nearestStart[3]{};
        float nearestTarget[3]{};
        if (dtStatusFailed(room.query->findNearestPoly(
                startPosition, searchExtents, &filter, &startRef, nearestStart)) ||
            dtStatusFailed(room.query->findNearestPoly(
                desiredPosition, searchExtents, &filter, &targetRef, nearestTarget)) ||
            !startRef || !targetRef)
        {
            return false;
        }

        dtPolyRef polygonPath[MAX_POLY_PATH]{};
        int polygonPathCount = 0;
        if (dtStatusFailed(room.query->findPath(
                startRef, targetRef, nearestStart, nearestTarget,
                &filter, polygonPath, &polygonPathCount, MAX_POLY_PATH)) ||
            polygonPathCount <= 0 || polygonPath[polygonPathCount - 1] != targetRef)
        {
            return false;
        }

        outPoint = { nearestTarget[0], nearestTarget[1], nearestTarget[2] };
        return true;
    }
}

namespace NpcNavMeshSystem
{
    // Возвращает следующую точку кешированного пути. При изменении цели,
    // комнаты, revision или fingerprint геометрии путь строится заново.
    bool FindNextWaypoint(
        ECSRegistry& registry,
        Entity npc,
        Entity currentRoom,
        const DirectX::XMFLOAT3& start,
        const DirectX::XMFLOAT3& target,
        std::uint32_t requestRevision,
        DirectX::XMFLOAT3& outWaypoint)
    {
        RoomNavMesh* room = GetOrBuildNavigationMesh(registry, currentRoom);
        if (!room)
        {
            g_paths.erase(npc);
            return false;
        }

        CachedPath& cache = g_paths[npc];
        const bool targetChanged = DistanceXZ(cache.requestedTarget, target) > 0.60f;
        // Не повторяем неудачный Detour-запрос каждый кадр. После первой ошибки
        // используется дешёвый direct fallback до смены цели/комнаты/revision
        // либо явного InvalidatePath при застревании.
        const bool pathWasNeverAttempted = !cache.buildAttempted;
        if (cache.room != currentRoom || cache.revision != requestRevision ||
            targetChanged || pathWasNeverAttempted)
        {
            cache.room = currentRoom;
            cache.requestedTarget = target;
            cache.revision = requestRevision;
            cache.points.clear();
            cache.nextPoint = 0;
            cache.buildAttempted = true;
            if (!BuildPath(*room, start, target, cache))
                return false;
        }

        while (cache.nextPoint + 1 < cache.points.size() &&
            DistanceXZ(start, cache.points[cache.nextPoint]) <= 0.35f)
        {
            ++cache.nextPoint;
        }

        if (cache.nextPoint >= cache.points.size())
            return false;

        outWaypoint = cache.points[cache.nextPoint];
        return true;
    }

    // Публичная обёртка проекции желаемой точки на достижимый остров сетки.
    bool FindReachablePoint(
        ECSRegistry& registry,
        Entity currentRoom,
        const DirectX::XMFLOAT3& start,
        const DirectX::XMFLOAT3& desiredPoint,
        DirectX::XMFLOAT3& outPoint)
    {
        RoomNavMesh* room = GetOrBuildNavigationMesh(registry, currentRoom);
        return room && FindConnectedProjectedPoint(
            *room, start, desiredPoint, outPoint);
    }

    // Ищет динамический AABB на коротком отрезке до desiredWaypoint и выдаёт
    // боковую точку объезда. Сторона запоминается, чтобы убрать дрожание.
    bool FindDynamicAvoidanceWaypoint(
        ECSRegistry& registry,
        Entity npc,
        Entity currentRoom,
        const DirectX::XMFLOAT3& start,
        const DirectX::XMFLOAT3& desiredWaypoint,
        DirectX::XMFLOAT3& outWaypoint)
    {
        if (auto debugIt = g_paths.find(npc); debugIt != g_paths.end())
        {
            debugIt->second.dynamicAvoidanceActive = false;
            debugIt->second.dynamicObstacle = INVALID_ENTITY;
            debugIt->second.dynamicObstacleEdges.clear();
        }

        const float toTargetX = desiredWaypoint.x - start.x;
        const float toTargetZ = desiredWaypoint.z - start.z;
        const float targetDistance = std::sqrt(
            toTargetX * toTargetX + toTargetZ * toTargetZ);
        if (targetDistance <= 0.10f)
            return false;

        const float directionX = toTargetX / targetDistance;
        const float directionZ = toTargetZ / targetDistance;
        const float lookAhead = (std::min)(3.0f, (std::max)(1.25f, targetDistance));

        Entity bestObstacle = INVALID_ENTITY;
        btRigidBody* bestBody = nullptr;
        btVector3 bestMinimum{};
        btVector3 bestMaximum{};
        DirectX::XMFLOAT3 bestCenter{};
        float bestRadius = 0.0f;
        float bestForward = (std::numeric_limits<float>::max)();
        float bestCross = 0.0f;

        for (auto& [entity, rigidBody] : registry.GetComponents<RigidBodyComponent>())
        {
            btRigidBody* body = rigidBody.body;
            if (!body || entity == npc || body->isStaticOrKinematicObject() ||
                body->getInvMass() <= 0.0f || !body->getCollisionShape())
            {
                continue;
            }

            btVector3 minimum{};
            btVector3 maximum{};
            body->getCollisionShape()->getAabb(body->getWorldTransform(), minimum, maximum);
            // Персонаж занимает примерно 1.8 м над Transform.position. Не
            // уклоняемся от предметов на другом этаже или высоко над головой.
            if (maximum.y() < start.y + 0.05f || minimum.y() > start.y + 1.80f)
                continue;

            const btVector3 velocity = body->getLinearVelocity();
            constexpr float predictionSeconds = 0.35f;
            const float centerX = (minimum.x() + maximum.x()) * 0.5f +
                velocity.x() * predictionSeconds;
            const float centerZ = (minimum.z() + maximum.z()) * 0.5f +
                velocity.z() * predictionSeconds;
            const float relativeX = centerX - start.x;
            const float relativeZ = centerZ - start.z;
            const float forward = relativeX * directionX + relativeZ * directionZ;
            const float halfX = (maximum.x() - minimum.x()) * 0.5f;
            const float halfZ = (maximum.z() - minimum.z()) * 0.5f;
            const float obstacleRadius = std::sqrt(halfX * halfX + halfZ * halfZ);
            constexpr float npcClearance = 0.62f;
            const float requiredClearance = obstacleRadius + npcClearance;
            if (forward < -requiredClearance * 0.25f ||
                forward > lookAhead + requiredClearance)
            {
                continue;
            }

            const float cross = directionX * relativeZ - directionZ * relativeX;
            if (std::abs(cross) >= requiredClearance || forward >= bestForward)
                continue;

            bestObstacle = entity;
            bestBody = body;
            bestMinimum = minimum;
            bestMaximum = maximum;
            bestCenter = { centerX, start.y, centerZ };
            bestRadius = requiredClearance;
            bestForward = forward;
            bestCross = cross;
        }

        if (bestObstacle == INVALID_ENTITY || !bestBody)
        {
            g_dynamicAvoidance.erase(npc);
            return false;
        }

        const auto now = std::chrono::steady_clock::now();
        DynamicAvoidanceMemory& memory = g_dynamicAvoidance[npc];
        float preferredSide = bestCross >= 0.0f ? -1.0f : 1.0f;
        if (memory.obstacle == bestObstacle && now < memory.keepSideUntil)
        {
            // Не меняем сторону объезда каждый кадр: без этой памяти NPC
            // дёргается влево-вправо перед катящимся шаром.
            preferredSide = memory.side;
        }
        else
        {
            memory.obstacle = bestObstacle;
            memory.side = preferredSide;
            memory.keepSideUntil = now + std::chrono::milliseconds(900);
        }

        RoomNavMesh* room = GetOrBuildNavigationMesh(registry, currentRoom);
        DirectX::XMFLOAT3 selectedPoint{};
        bool pointFound = false;
        for (int sideAttempt = 0; sideAttempt < 2 && !pointFound; ++sideAttempt)
        {
            const float side = sideAttempt == 0 ? preferredSide : -preferredSide;
            // Перпендикуляр (-z, x) уводит капсулу вбок, небольшой forward
            // offset не позволяет застыть точно напротив центра препятствия.
            DirectX::XMFLOAT3 candidate = {
                bestCenter.x + (-directionZ) * side * (bestRadius + 0.30f) +
                    directionX * 0.55f,
                start.y,
                bestCenter.z + directionX * side * (bestRadius + 0.30f) +
                    directionZ * 0.55f
            };

            if (room)
                pointFound = FindConnectedProjectedPoint(*room, start, candidate, selectedPoint);
            else
            {
                selectedPoint = candidate;
                pointFound = true;
            }

            if (pointFound && DistanceXZ(start, selectedPoint) < 0.35f)
                pointFound = false;
            if (pointFound)
            {
                memory.side = side;
                memory.keepSideUntil = now + std::chrono::milliseconds(900);
            }
        }

        if (!pointFound)
            return false;

        BuildGeometry::ObstacleBox dynamicBox;
        dynamicBox.minimum[0] = bestMinimum.x();
        dynamicBox.minimum[1] = bestMinimum.y();
        dynamicBox.minimum[2] = bestMinimum.z();
        dynamicBox.maximum[0] = bestMaximum.x();
        dynamicBox.maximum[1] = bestMaximum.y();
        dynamicBox.maximum[2] = bestMaximum.z();
        // GetOrBuildRoom выше мог автоматически перестроить комнату и удалить
        // её старые пути, поэтому ссылку на cache получаем только сейчас.
        CachedPath& debugCache = g_paths[npc];
        debugCache.dynamicAvoidanceActive = true;
        debugCache.dynamicObstacle = bestObstacle;
        debugCache.avoidanceStart = start;
        debugCache.avoidancePoint = selectedPoint;
        AppendDebugBoxEdges(debugCache.dynamicObstacleEdges, dynamicBox);
        outWaypoint = selectedPoint;
        return true;
    }

    // Удаляет только путь и память объезда одного NPC.
    void InvalidatePath(Entity npc)
    {
        const auto pathIt = g_paths.find(npc);
        if (pathIt != g_paths.end())
        {
            // Неудачная сборка комнаты кешируется, чтобы не повторять тяжёлую
            // работу каждый кадр. Force repath обязан удалить и этот плохой кеш,
            // иначе после исправления коллайдера UI продолжит показывать старую
            // ошибку вплоть до полной перезагрузки сцены.
            if (pathIt->second.room == INVALID_ENTITY && g_worldMesh &&
                (!g_worldMesh->mesh || !g_worldMesh->query))
            {
                g_worldMesh.reset();
            }
            const auto roomIt = g_roomMeshes.find(pathIt->second.room);
            if (roomIt != g_roomMeshes.end() &&
                (!roomIt->second || !roomIt->second->mesh || !roomIt->second->query))
            {
                g_roomMeshes.erase(roomIt);
            }
            g_paths.erase(pathIt);
        }
        g_dynamicAvoidance.erase(npc);
    }

    // Полный сброс кешей после загрузки сцены или ручной команды Rebuild.
    void InvalidateAll()
    {
        g_paths.clear();
        g_roomMeshes.clear();
        g_worldMesh.reset();
        g_dynamicAvoidance.clear();
        OutputDebugStringA("[NPC NavMesh] All cached meshes invalidated.\n");
    }

    // Собирает безопасную копию runtime-данных для UI и debug renderer.
    NpcNavMeshDebugInfo GetDebugInfo(Entity npc)
    {
        NpcNavMeshDebugInfo info;
        const auto pathIt = g_paths.find(npc);
        if (pathIt == g_paths.end())
        {
            // Поиск мог построить сетку, но не найти ни одного маршрута.
            // В таком случае всё равно показываем сетку полигона и её ошибку.
            if (g_worldMesh)
            {
                info.roomMeshBuilt = g_worldMesh->mesh != nullptr;
                info.floorOnlyFallback = g_worldMesh->floorOnlyFallback;
                info.polygonCount = g_worldMesh->polygonCount;
                info.meshEdges = g_worldMesh->debugMeshEdges;
                info.obstacleEdges = g_worldMesh->debugObstacleEdges;
                info.status = g_worldMesh->status;
            }
            return info;
        }

        const CachedPath& cache = pathIt->second;
        info.room = cache.room;
        info.waypointCount = static_cast<int>(cache.points.size());
        info.currentWaypoint = static_cast<int>(cache.nextPoint);
        info.path = cache.points;
        info.status = cache.status;
        info.dynamicAvoidanceActive = cache.dynamicAvoidanceActive;
        info.dynamicObstacle = cache.dynamicObstacle;
        info.avoidanceStart = cache.avoidanceStart;
        info.avoidancePoint = cache.avoidancePoint;
        if (cache.dynamicAvoidanceActive)
        {
            info.status += "; avoiding dynamic entity " +
                std::to_string(cache.dynamicObstacle);
            info.obstacleEdges.insert(
                info.obstacleEdges.end(),
                cache.dynamicObstacleEdges.begin(),
                cache.dynamicObstacleEdges.end());
        }

        const auto roomIt = g_roomMeshes.find(cache.room);
        const RoomNavMesh* debugMesh = cache.room == INVALID_ENTITY
            ? g_worldMesh.get()
            : roomIt != g_roomMeshes.end() ? roomIt->second.get() : nullptr;
        if (debugMesh)
        {
            info.roomMeshBuilt = debugMesh->mesh != nullptr;
            info.floorOnlyFallback = debugMesh->floorOnlyFallback;
            info.polygonCount = debugMesh->polygonCount;
            info.meshEdges = debugMesh->debugMeshEdges;
            info.obstacleEdges.insert(
                info.obstacleEdges.begin(),
                debugMesh->debugObstacleEdges.begin(),
                debugMesh->debugObstacleEdges.end());
            if (!info.roomMeshBuilt)
                info.status = debugMesh->status;
        }
        return info;
    }

    NpcNavMeshDebugDrawSettings& GetDebugDrawSettings()
    {
        return g_debugDrawSettings;
    }
}

#endif
