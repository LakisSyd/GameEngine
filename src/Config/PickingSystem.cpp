#include "PickingSystem.h"
#include "GameSettings.h"
#include "Gameplay/Power/OxygenGeneratorComponent.h"
#include <DirectXCollision.h>
#include <cmath>
#include <limits>
using namespace DirectX;

bool PickingSystem::RayIntersectsSphere(
    const CameraClass::Ray& ray,
    const XMFLOAT3& center,
    float radius) const
{
    XMVECTOR rayOrigin = XMLoadFloat3(&ray.origin);
    XMVECTOR rayDir = XMLoadFloat3(&ray.direction);
    XMVECTOR sphereCenter = XMLoadFloat3(&center);

    XMVECTOR L = XMVectorSubtract(sphereCenter, rayOrigin);
    float t = XMVectorGetX(XMVector3Dot(L, rayDir));

    if (t < 0.0f) return false;

    XMVECTOR closest = XMVectorAdd(rayOrigin, XMVectorScale(rayDir, t));
    float distance = XMVectorGetX(XMVector3Length(XMVectorSubtract(closest, sphereCenter)));

    return distance <= radius;
}

bool PickingSystem::RayIntersectsModelBounds(
    const CameraClass::Ray& ray,
    const XMMATRIX& world,
    const XMFLOAT3& localMin,
    const XMFLOAT3& localMax,
    float& outDistance) const
{
    const XMMATRIX inverseWorld = XMMatrixInverse(nullptr, world);
    const XMVECTOR worldOrigin = XMLoadFloat3(&ray.origin);
    const XMVECTOR localOrigin = XMVector3TransformCoord(worldOrigin, inverseWorld);
    const XMVECTOR localDirection = XMVector3Normalize(
        XMVector3TransformNormal(XMLoadFloat3(&ray.direction), inverseWorld));

    BoundingBox bounds;
    bounds.Center = {
        (localMin.x + localMax.x) * 0.5f,
        (localMin.y + localMax.y) * 0.5f,
        (localMin.z + localMax.z) * 0.5f
    };
    bounds.Extents = {
        (localMax.x - localMin.x) * 0.5f,
        (localMax.y - localMin.y) * 0.5f,
        (localMax.z - localMin.z) * 0.5f
    };

    float localDistance = 0.0f;
    if (!bounds.Intersects(localOrigin, localDirection, localDistance))
        return false;

    const XMVECTOR localHit = XMVectorAdd(
        localOrigin,
        XMVectorScale(localDirection, localDistance));
    const XMVECTOR worldHit = XMVector3TransformCoord(localHit, world);

    outDistance = XMVectorGetX(XMVector3Length(
        XMVectorSubtract(worldHit, worldOrigin)));
    return true;
}

bool PickingSystem::RayIntersectsModelTriangles(
    const CameraClass::Ray& ray,
    const XMMATRIX& world,
    const AssimpModelClass& model,
    float& outDistance) const
{
    // Сначала луч переносится в локальное пространство модели: вершины меша
    // там уже лежат, поэтому поворот и масштаб объекта учитываются корректно.
    const XMMATRIX inverseWorld = XMMatrixInverse(nullptr, world);
    const XMVECTOR worldOrigin = XMLoadFloat3(&ray.origin);
    const XMVECTOR localOrigin = XMVector3TransformCoord(worldOrigin, inverseWorld);
    const XMVECTOR localDirection = XMVector3Normalize(
        XMVector3TransformNormal(XMLoadFloat3(&ray.direction), inverseWorld));

    std::vector<XMFLOAT3> triangles;
    model.GetRawTriangles(triangles);

    float closestLocalDistance = (std::numeric_limits<float>::max)();
    for (size_t i = 0; i + 2 < triangles.size(); i += 3)
    {
        float localDistance = 0.0f;
        if (TriangleTests::Intersects(
                localOrigin,
                localDirection,
                XMLoadFloat3(&triangles[i]),
                XMLoadFloat3(&triangles[i + 1]),
                XMLoadFloat3(&triangles[i + 2]),
                localDistance) &&
            localDistance < closestLocalDistance)
        {
            closestLocalDistance = localDistance;
        }
    }

    if (closestLocalDistance == (std::numeric_limits<float>::max)())
        return false;

    const XMVECTOR localHit = XMVectorAdd(
        localOrigin,
        XMVectorScale(localDirection, closestLocalDistance));
    const XMVECTOR worldHit = XMVector3TransformCoord(localHit, world);

    outDistance = XMVectorGetX(XMVector3Length(
        XMVectorSubtract(worldHit, worldOrigin)));
    return true;
}

float PickingSystem::CalculateSpriteRadius(const WorldSpriteClass* sprite) const
{
    if (!sprite) return 0.0f;
    XMFLOAT2 scale = sprite->GetScale();
    float factor = Settings::SPRITE_RADIUS_FACTOR;
    return sqrtf(scale.x * scale.x + scale.y * scale.y) * factor;
}

PickingSystem::PickResult PickingSystem::PickAt(
    int screenX, int screenY,
    CameraClass& camera,
    const XMMATRIX& viewMatrix,      // < Принимаем вид
    const XMMATRIX& projectionMatrix,// < Принимаем проекцию
    SceneManager& scene,
    const std::vector<GameObject>& lightSpheres,
    int screenWidth,
    int screenHeight)
{
    PickResult result;

    // ?? Создаём луч, используя переданные матрицы
    auto ray = camera.CreatePickingRay(
        screenX, screenY,
        screenWidth, screenHeight,
        viewMatrix,          // < Передаём вид
        projectionMatrix     // < Передаём проекцию
    );



    // 1. Источники света (приоритет 1) — остаются на int index
    for (size_t i = 0; i < lightSpheres.size(); ++i)
    {
        if (RayIntersectsSphere(ray, lightSpheres[i].GetPosition(), Settings::LIGHT_SPHERE_RADIUS))
        {
            result.type = PickedType::Light;
            result.index = static_cast<int>(i);
            result.worldPosition = lightSpheres[i].GetPosition();
            return result;
        }
    }

    // 2. Спрайты (приоритет 2) — остаются на int index
    for (size_t i = 0; i < scene.GetSpriteCount(); ++i)
    {
        auto* sprite = scene.GetSprite(static_cast<int>(i));
        if (!sprite) continue;

        float radius = CalculateSpriteRadius(sprite);
        if (RayIntersectsSphere(ray, sprite->GetPosition(), radius))
        {
            result.type = PickedType::Sprite;
            result.index = static_cast<int>(i);
            result.worldPosition = sprite->GetPosition();
            return result;
        }
    }

    // 3. Объекты (приоритет 3)
    // НЕДЕЛЯ 4: итерируем ECS напрямую — не вызываем GetObject(int)
    // result.index = Entity приведённый к int (для EditorUI / ApplicationClass)
    auto& registry = scene.GetECSRegistry();
    auto& meshPool = registry.GetComponents<MeshRendererComponent>();

    float closestDistance = (std::numeric_limits<float>::max)();
    float closestVolume = (std::numeric_limits<float>::max)();
    constexpr float distanceTieEpsilon = 0.01f;

    for (auto& [entity, mesh] : meshPool)
    {
        // Только активные объекты участвуют в picking
        if (!scene.IsObjectActive(entity)) continue;

        auto* transform = scene.GetObjectTransform(entity);
        if (!transform) continue;

        const XMMATRIX world = transform->GetWorldMatrix();

        float hitDistance = 0.0f;
        float volume = Settings::OBJECT_PICK_RADIUS * Settings::OBJECT_PICK_RADIUS * Settings::OBJECT_PICK_RADIUS;
        bool hit = false;

        if (mesh.model)
        {
            XMFLOAT3 localMin;
            XMFLOAT3 localMax;
            mesh.model->GetBoundingBox(&localMin, &localMax);

            // AABB - только быстрая предварительная проверка. После неё луч
            // проверяется по реальным треугольникам: террейн больше не
            // «накрывает» лежащий на нём небольшой объект при взгляде сверху.
            if (RayIntersectsModelBounds(ray, world, localMin, localMax, hitDistance))
            {
                hit = RayIntersectsModelTriangles(ray, world, *mesh.model, hitDistance);

                const float sx = XMVectorGetX(XMVector3Length(world.r[0]));
                const float sy = XMVectorGetX(XMVector3Length(world.r[1]));
                const float sz = XMVectorGetX(XMVector3Length(world.r[2]));
                volume = fabsf((localMax.x - localMin.x) * sx *
                               (localMax.y - localMin.y) * sy *
                               (localMax.z - localMin.z) * sz);
            }
        }
        else
        {
            hit = RayIntersectsSphere(
                ray, transform->position, Settings::OBJECT_PICK_RADIUS);

            XMVECTOR toObject = XMVectorSubtract(
                XMLoadFloat3(&transform->position), XMLoadFloat3(&ray.origin));
            hitDistance = XMVectorGetX(
                XMVector3Dot(toObject, XMLoadFloat3(&ray.direction)));
        }

        const bool isCloser = hitDistance < closestDistance - distanceTieEpsilon;
        const bool sameSurfaceButSmaller =
            fabsf(hitDistance - closestDistance) <= distanceTieEpsilon &&
            volume < closestVolume;

        if (hit && (isCloser || sameSurfaceButSmaller))
        {
            closestDistance = hitDistance;
            closestVolume = volume;
            result.type = PickedType::Object;
            result.index = static_cast<int>(entity); // Entity -> int for legacy
            result.worldPosition = {
                ray.origin.x + ray.direction.x * hitDistance,
                ray.origin.y + ray.direction.y * hitDistance,
                ray.origin.z + ray.direction.z * hitDistance
            };
        }
    }

    if (result.type == PickedType::Object)
    {
        Entity picked = static_cast<Entity>(result.index);

        // Resolve visual panel batch picks back to the real physical panel entity.
        if (auto* batch = registry.GetComponent<ShipPanelVisualBatchComponent>(picked))
        {
            Entity closestPanel = INVALID_ENTITY;
            float closestPanelDistance = (std::numeric_limits<float>::max)();
            XMFLOAT3 closestPanelWorldPosition = result.worldPosition;

            auto& rigidBodies = registry.GetComponents<RigidBodyComponent>();
            for (auto& [panelEntity, rb] : rigidBodies)
            {
                if (!registry.HasComponent<HullPanelComponent>(panelEntity))
                    continue;

                auto* part = registry.GetComponent<ShipPartComponent>(panelEntity);
                if (!part || part->root != batch->shipRoot)
                    continue;

                auto* transform = registry.GetComponent<TransformComponent>(panelEntity);
                if (!transform)
                    continue;

                AssimpModelClass* pickModel = nullptr;
                if (rb.useSeparateCollisionMesh && rb.collisionModel)
                    pickModel = rb.collisionModel;
                else if (auto* mesh = registry.GetComponent<MeshRendererComponent>(panelEntity))
                    pickModel = mesh->model;

                if (!pickModel)
                    continue;

                XMFLOAT3 localMin{};
                XMFLOAT3 localMax{};
                pickModel->GetBoundingBox(&localMin, &localMax);

                const XMMATRIX panelWorld = transform->GetWorldMatrix();
                float panelDistance = 0.0f;
                if (!RayIntersectsModelBounds(ray, panelWorld, localMin, localMax, panelDistance))
                    continue;

                if (!RayIntersectsModelTriangles(ray, panelWorld, *pickModel, panelDistance))
                    continue;

                if (panelDistance < closestPanelDistance)
                {
                    closestPanelDistance = panelDistance;
                    closestPanel = panelEntity;
                    closestPanelWorldPosition = {
                        ray.origin.x + ray.direction.x * panelDistance,
                        ray.origin.y + ray.direction.y * panelDistance,
                        ray.origin.z + ray.direction.z * panelDistance
                    };
                }
            }

            if (closestPanel != INVALID_ENTITY)
            {
                picked = closestPanel;
                result.index = static_cast<int>(closestPanel);
                result.worldPosition = closestPanelWorldPosition;
            }
        }
        if (auto* selection = registry.GetComponent<EditorSelectionComponent>(picked))
        {
            const bool editableShipPart =
                registry.HasComponent<HullPanelComponent>(picked) ||
                registry.HasComponent<HullBreachComponent>(picked) ||
                registry.HasComponent<DoorComponent>(picked) ||
                registry.HasComponent<RoomComponent>(picked) ||
                registry.HasComponent<OxygenGeneratorComponent>(picked);

            // ShipRoot is still selectable from the outliner, but direct viewport
            // clicks on editable ship parts select the actual part for tuning.
            if (selection->redirectTo != INVALID_ENTITY && !editableShipPart)
                result.index = static_cast<int>(selection->redirectTo);
            else if (!selection->selectable && !editableShipPart)
                result = {};
        }
    }

    return result; // Ничего не найдено
}
