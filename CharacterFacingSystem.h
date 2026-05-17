#pragma once

#include "Components.h"
#include "ECSRegistry.h"
#include <DirectXMath.h>
#include <cmath>
constexpr float ModelForwardOffset = DirectX::XM_PI; // 180 degrees, model faces backward.

class CharacterFacingSystem
{
public:
    static void Update(ECSRegistry& registry, float deltaTime)
    {
        registry.ForEach<CharacterMovement>(
            [&](Entity entity, CharacterMovement& movement)
            {
                if (!movement.isMoving)
                    return;

                auto* transform = registry.GetComponent<TransformComponent>(entity);
                if (!transform)
                    return;

                DirectX::XMFLOAT3 dir = movement.walkDirection;

                const float lenSq = dir.x * dir.x + dir.z * dir.z;
                if (lenSq < 0.0001f)
                    return;

                const float targetYaw =
                    atan2f(dir.x, dir.z) + DirectX::XM_PI;

                const float currentYaw = transform->rotation.y;

                float deltaYaw = targetYaw - currentYaw;
                deltaYaw = NormalizeAngle(deltaYaw);

                const float maxStep =
                    movement.rotationSpeed * deltaTime;

                deltaYaw = std::clamp(
                    deltaYaw,
                    -maxStep,
                    maxStep
                );

                transform->rotation.y += deltaYaw;
            });
    }
private:
    static float NormalizeAngle(float angle)
    {
        while (angle > DirectX::XM_PI)
            angle -= DirectX::XM_2PI;

        while (angle < -DirectX::XM_PI)
            angle += DirectX::XM_2PI;

        return angle;
    }
};