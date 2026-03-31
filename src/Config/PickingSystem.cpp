#include "PickingSystem.h"
#include "GameSettings.h"
#include <DirectXCollision.h>
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
    const XMMATRIX& viewMatrix,      // ← Принимаем вид
    const XMMATRIX& projectionMatrix,// ← Принимаем проекцию
    SceneManager& scene,
    const std::vector<GameObject>& lightSpheres,
    int screenWidth,
    int screenHeight)
{
    PickResult result;

    // 🔥 Создаём луч, используя переданные матрицы
    auto ray = camera.CreatePickingRay(
        screenX, screenY,
        screenWidth, screenHeight,
        viewMatrix,          // ← Передаём вид
        projectionMatrix     // ← Передаём проекцию
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

    for (auto& [entity, mesh] : meshPool)
    {
        // Только активные объекты участвуют в picking
        if (!scene.IsObjectActive(entity)) continue;

        auto* transform = scene.GetObjectTransform(entity);
        if (!transform) continue;

        if (RayIntersectsSphere(ray, transform->position, Settings::OBJECT_PICK_RADIUS))
        {
            result.type = PickedType::Object;
            result.index = static_cast<int>(entity); // Entity → int для legacy
            result.worldPosition = transform->position;
            return result;
        }
    }

    return result; // Ничего не найдено
}