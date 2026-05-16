#pragma once
#include <ECSRegistry.h>

class CharacterControllerSystem
{
public:
    static void Update(ECSRegistry& registry, float dt)
    {
        registry.ForEach<CharacterController>(
            [&](Entity entity, CharacterController& controller)
            {
                auto* movement = registry.GetComponent<CharacterMovement>(entity);
                if (!movement || !controller.controller)
                    return;

                btVector3 walk(
                    movement->walkDirection.x * movement->currentSpeed * dt,
                    movement->walkDirection.y * movement->currentSpeed * dt,
                    movement->walkDirection.z * movement->currentSpeed * dt
                );

                controller.controller->setWalkDirection(walk);

                if (movement->wantJump && controller.controller->canJump())
                {
                    controller.controller->jump();
                }

                movement->wantJump = false;
            });
    }

    static void UpdateCharacterStep(ECSRegistry& registry)
    {
        registry.ForEach<CharacterController>(
            [&](Entity entity, CharacterController& controller)
            {
                auto* transform = registry.GetComponent<TransformComponent>(entity);
                if (!transform || !controller.ghostObj)
                    return;

                btTransform btTrans = controller.ghostObj->getWorldTransform();
                btVector3 pos = btTrans.getOrigin();

                transform->position = {
                    pos.getX(),
                    pos.getY(),
                    pos.getZ()
                };
            });
    }
};