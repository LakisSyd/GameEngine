#pragma once

#include "Components.h"
#include "ECSRegistry.h"
#include "GameplayInputSystem.h"
#include <DirectXMath.h>
#include "PlayerComponents.h"
class CharacterMovementSystem
{
public:
    static void Update(ECSRegistry& registry, float deltaTime, const GameInputFrame& input, float cameraYaw)
    {
        (void)deltaTime;

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

        if (input.Down(GameAction::MoveForward)) direction = XMVectorAdd(direction, forward);
        if (input.Down(GameAction::MoveBackward)) direction = XMVectorSubtract(direction, forward);
        if (input.Down(GameAction::MoveRight)) direction = XMVectorAdd(direction, right);
        if (input.Down(GameAction::MoveLeft)) direction = XMVectorSubtract(direction, right);

        const float lengthSq = XMVectorGetX(XMVector3LengthSq(direction));

        movement->isMoving = lengthSq > 0.0001f;
        movement->walkDirection = { 0.0f, 0.0f, 0.0f };
        movement->currentSpeed = 0.0f;
        movement->wantJump = input.Down(GameAction::Jump);

        // Pressed срабатывает только один кадр в момент нажатия C.
        // Поэтому каждое новое нажатие переключает сохранённое состояние:
        // стоял -> присел, приседал -> встал.
        if (input.Pressed(GameAction::Crouch))
            movement->isCrouching = !movement->isCrouching;

        movement->isRunning = false;
        movement->wantsRun = false;

        if (!movement->isMoving)
        {
            // Стоящий персонаж остаётся в выбранной позе приседа.
            // isCrouching здесь не сбрасываем: анимация прочитает этот флаг.
            movement->locomotionState = PlayerLocomotionState::Idle;
            return;
        }

        direction = XMVector3Normalize(direction);
        XMStoreFloat3(&movement->walkDirection, direction);

        if (movement->isCrouching)
        {
            // Присевший персонаж движется вдвое медленнее обычного шага.
            // moveSpeed не изменяем: это настроечное значение обычной ходьбы.
            movement->currentSpeed = movement->moveSpeed * 0.5f;
            movement->wantsRun = false;
            movement->isRunning = false;
            movement->locomotionState = PlayerLocomotionState::Walk;

            // Не выполняем расположенную ниже логику Shift и стамины,
            // иначе она снова перезапишет скорость и разрешит ускорение.
            return;
        }

        auto* vitals = registry.GetComponent<CharacterVitalsComponent>(player);

        const bool shiftDown = input.Down(GameAction::Sprint);
        if (vitals)
        {
            if (vitals->stamina <= 0.0f)
                vitals->staminaExhausted = true;
            else if (vitals->staminaExhausted &&
                vitals->stamina >= vitals->sprintStartThreshold)
                vitals->staminaExhausted = false;
        }

        const bool hasEnoughStaminaToRun = vitals &&
            !vitals->staminaExhausted && vitals->stamina > 0.0f;
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
