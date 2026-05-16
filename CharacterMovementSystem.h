#pragma once

#include "Components.h"
#include "ECSRegistry.h"
#include "InputClass.h"
#include <DirectXMath.h>

class CharacterMovementSystem
{
public:
    static void Update(ECSRegistry& registry, float deltaTime, InputClass* input)
    {
        if (!input) return;

        Entity player = INVALID_ENTITY;

        registry.ForEach<PlayerGameTag>([&](Entity entity, PlayerGameTag& tag)
            {
                player = entity;
            });

        if (player == INVALID_ENTITY)
            return;

        auto* transform = registry.GetComponent<TransformComponent>(player);
        auto* movement = registry.GetComponent<CharacterMovement>(player);

        if (!transform || !movement)
            return;

        using namespace DirectX;

        XMFLOAT3 inputDirection = { 0.0f, 0.0f, 0.0f };

        if (input->IsKeyDown('W')) inputDirection.z += 1.0f;
        if (input->IsKeyDown('S')) inputDirection.z -= 1.0f;
        if (input->IsKeyDown('A')) inputDirection.x -= 1.0f;
        if (input->IsKeyDown('D')) inputDirection.x += 1.0f;

        XMVECTOR direction = XMLoadFloat3(&inputDirection);
        const float lengthSq = XMVectorGetX(XMVector3LengthSq(direction));

        movement->isMoving = lengthSq > 0.0001f;
        movement->walkDirection = { 0.0f, 0.0f, 0.0f };
        movement->currentSpeed = 0.0f;

        if (!movement->isMoving)
            return;

        direction = XMVector3Normalize(direction);
        XMStoreFloat3(&movement->walkDirection, direction);

        constexpr unsigned int KEY_SHIFT = 0x10;
        const bool running = input->IsKeyDown(KEY_SHIFT);
        const float speed = running ? movement->runSpeed : movement->moveSpeed;

        movement->currentSpeed = speed;

        XMVECTOR position = XMLoadFloat3(&transform->position);
        position = XMVectorAdd(position, XMVectorScale(direction, speed * deltaTime));
        XMStoreFloat3(&transform->position, position);
    }
};
