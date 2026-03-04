#pragma once
#include <DirectXMath.h>
#include <vector>
#include "cameraclass.h"
#include "SceneManager.h"
#include "WorldSpriteClass.h"
#include "GameObject.h"

using namespace DirectX;

class PickingSystem {
public:
    enum class PickedType { None, Light, Object, Sprite };

    struct PickResult {
        PickedType type = PickedType::None;
        int index = -1;
        XMFLOAT3 worldPosition;

        bool IsValid() const { return type != PickedType::None; }
    };

    // PickingSystem.h
    PickResult PickAt(
        int screenX, int screenY,
        CameraClass& camera,
        const XMMATRIX& viewMatrix,      // ← Вид отдельно
        const XMMATRIX& projectionMatrix,// ← Проекция отдельно ❗
        SceneManager& scene,
        const std::vector<GameObject>& lightSpheres,
        int screenWidth,
        int screenHeight);

private:
    bool RayIntersectsSphere(
        const CameraClass::Ray& ray,
        const XMFLOAT3& center,
        float radius) const;

    float CalculateSpriteRadius(const WorldSpriteClass* sprite) const;
};