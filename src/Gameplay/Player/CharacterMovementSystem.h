#pragma once

#include "Components.h"
#include "ECSRegistry.h"
#include "InputClass.h"
#include <DirectXMath.h>
#include "PlayerComponents.h"
class CharacterMovementSystem
{
public:
    static void Update(ECSRegistry& registry, float deltaTime, InputClass* input, float cameraYaw)
    {
        (void)deltaTime;
        if (!input) return;

        Entity player = INVALID_ENTITY;

        registry.ForEach<PlayerGameTag>([&](Entity entity, PlayerGameTag&)
        {
            player = entity;
        });

        if (player == INVALID_ENTITY)
            return;

        auto* movement = registry.GetComponent<CharacterMovement>(player);
        if (!movement)
            return;

        using namespace DirectX;

        const float yawRad = XMConvertToRadians(cameraYaw);

        const XMVECTOR forward = XMVectorSet(
            sinf(yawRad),
            0.0f,
            cosf(yawRad),
            0.0f
        );

        const XMVECTOR right = XMVectorSet(
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
        constexpr unsigned int KEY_SPACE = 0x20;
        movement->wantJump = input->IsKeyDown(KEY_SPACE);

        movement->isRunning = false;
        movement->wantsRun = false;
        if (!movement->isMoving)
        {
            movement->locomotionState = PlayerLocomotionState::Idle;
            return;
        }

      

        direction = XMVector3Normalize(direction);
        XMStoreFloat3(&movement->walkDirection, direction);

        auto* playerComp = registry.GetComponent<PlayerComponent>(player);

        constexpr unsigned int KEY_SHIFT = 0x10;
        const bool shiftDown = input->IsKeyDown(KEY_SHIFT);
        const bool hasEnoughStaminaToRun = playerComp && playerComp->stamina > 5.0f;
        movement->wantsRun = movement->isMoving && shiftDown;

        movement->isRunning = movement->wantsRun && hasEnoughStaminaToRun;
        movement->currentSpeed = movement->isRunning ? movement->runSpeed : movement->moveSpeed;

        if (movement->isRunning)
        {
            movement->locomotionState = PlayerLocomotionState::Run;
            movement->currentSpeed = movement->runSpeed;
        }
        else if (movement->wantsRun && !hasEnoughStaminaToRun)
        {
            movement->locomotionState = PlayerLocomotionState::Exhausted;
            movement->currentSpeed = movement->moveSpeed;
        }
        else
        {
            movement->locomotionState = PlayerLocomotionState::Walk;
            movement->currentSpeed = movement->moveSpeed;
        }
    }
};
