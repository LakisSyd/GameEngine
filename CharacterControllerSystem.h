#pragma once
#include <ECSRegistry.h>

class CharacterControllerSystem
{
public:
    static void Update(ECSRegistry& registry, float dt)
    {
        (void)dt;
        registry.ForEach<CharacterController>(
            [&](Entity entity, CharacterController& controller)
            {
                auto* movement = registry.GetComponent<CharacterMovement>(entity);
                if (!movement || !controller.controller)
                    return;

                constexpr float fixedPhysicsStep = 1.0f / 60.0f;
                btVector3 walk(
                    movement->walkDirection.x * movement->currentSpeed * fixedPhysicsStep,
                    movement->walkDirection.y * movement->currentSpeed * fixedPhysicsStep,
                    movement->walkDirection.z * movement->currentSpeed * fixedPhysicsStep
                );

                controller.controller->setWalkDirection(walk);

                

                if (movement->wantJump && controller.controller->canJump())
                {
                    controller.controller->jump();
                }

                movement->wantJump = false;
            });
    }
};
