#pragma once

#include "AnimationComponent.h"
#include "Components.h"
#include "ECSRegistry.h"
#include "ECSTypes.h"

class CharacterAnimationControllerSystem
{
public:
    static void Update(ECSRegistry& registry, float deltaTime)
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
        auto* anim = registry.GetComponent<AnimationComponent>(player);

        if (!movement || !anim)
            return;

        const int desiredAnimationIndex = SelectMovementAnimation(*movement);

        anim->isPlaying = true;

        if (anim->animationIndex != desiredAnimationIndex)
        {
            anim->animationIndex = desiredAnimationIndex;
            anim->currentTime = 0.0f;
        }
    }

private:
    static int SelectMovementAnimation(const CharacterMovement& movement)
    {
        constexpr int IdleAnimation = 0;
        constexpr int WalkAnimation = 1;
        constexpr int RunAnimation = 2;

        if (!movement.isMoving)
            return IdleAnimation;

        if (movement.currentSpeed > movement.moveSpeed)
            return RunAnimation;

        return WalkAnimation;
    }
};

using AnimationController = CharacterAnimationControllerSystem;
