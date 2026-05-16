#pragma once

#include "Components.h"
#include "ECSRegistry.h"
#include "InputClass.h"
#include <DirectXMath.h>

class CharacterMovementSystem
{
public:
    static void Update(ECSRegistry& registry, float deltaTime, InputClass* input, float cameraYaw)
    {
        if (!input) return;

        Entity player = INVALID_ENTITY;

        registry.ForEach<PlayerGameTag>([&](Entity entity, PlayerGameTag& tag)
            {
                player = entity;
            });

        if (player == INVALID_ENTITY)
            return;

      
        auto* movement = registry.GetComponent<CharacterMovement>(player);

        

        using namespace DirectX;

        float yawRad = XMConvertToRadians(cameraYaw);

        XMVECTOR forward = XMVectorSet(
            sinf(yawRad),
            0.0f,
            cosf(yawRad),
            0.0f
        );

        XMVECTOR right = XMVectorSet(
            cosf(yawRad),
            0.0f,
            -sinf(yawRad),
            0.0f
        );

        XMVECTOR direction = XMVectorZero();

        if (input->IsKeyDown('W')) direction = XMVectorAdd(direction, forward);
        if (input->IsKeyDown('S')) direction = XMVectorSubtract(direction, forward);
        if (input->IsKeyDown('D')) direction = XMVectorAdd(direction, right);
        if (input->IsKeyDown('A')) direction = XMVectorSubtract(direction, right);
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

       
    }
};
