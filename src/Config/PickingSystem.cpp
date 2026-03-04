#include "PickingSystem.h"
#include "GameSettings.h"
#include <DirectXCollision.h>
using namespace DirectX;

bool PickingSystem::RayIntersectsSphere(
    const CameraClass::Ray& ray,
    const XMFLOAT3& center,
    float radius) const
{
    // Используем DirectXMath для надёжности
    XMVECTOR rayOrigin = XMLoadFloat3(&ray.origin);
    XMVECTOR rayDir = XMLoadFloat3(&ray.direction);
    XMVECTOR sphereCenter = XMLoadFloat3(&center);

    // Формула: расстояние от центра сферы до луча
    XMVECTOR L = sphereCenter - rayOrigin;
    float t = XMVectorGetX(XMVector3Dot(L, rayDir));

    if (t < 0.0f) return false; // Сфера позади луча

    XMVECTOR closest = rayOrigin + rayDir * t;
    float distance = XMVectorGetX(XMVector3Length(closest - sphereCenter));

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


   

    // 1. Проверяем сферы света (приоритет 1)
    for (size_t i = 0; i < lightSpheres.size(); ++i) {
        if (RayIntersectsSphere(ray, lightSpheres[i].GetPosition(), Settings::LIGHT_SPHERE_RADIUS)) {
            result.type = PickedType::Light;
            result.index = static_cast<int>(i);
            result.worldPosition = lightSpheres[i].GetPosition();
            return result;
        }
    }

    // 2. Проверяем спрайты (приоритет 2)
    for (size_t i = 0; i < scene.GetSpriteCount(); ++i) {
        auto* sprite = scene.GetSprite(static_cast<int>(i));
        if (!sprite) continue;

        float radius = CalculateSpriteRadius(sprite);
        if (RayIntersectsSphere(ray, sprite->GetPosition(), radius)) {
            result.type = PickedType::Sprite;
            result.index = static_cast<int>(i);
            result.worldPosition = sprite->GetPosition();
            return result;
        }
    }

    // 3. Проверяем объекты (приоритет 3)
    for (size_t i = 0; i < scene.GetObjectCount(); ++i) {
        auto* obj = scene.GetObject(static_cast<int>(i));
        if (!obj) continue;

        if (RayIntersectsSphere(ray, obj->GetPosition(), Settings::OBJECT_PICK_RADIUS)) {
            result.type = PickedType::Object;
            result.index = static_cast<int>(i);
            result.worldPosition = obj->GetPosition();
            return result;
        }
    }

    return result; // Ничего не найдено
}