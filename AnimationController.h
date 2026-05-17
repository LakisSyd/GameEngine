#pragma once

#include "AnimationComponent.h"
#include "Components.h"
#include "ECSRegistry.h"
#include "ECSTypes.h"

class CharacterAnimationControllerSystem
{
public:



    static AnimationBinding* FindBinding(AnimationComponent& anim, AnimationState state)
    {
        for (auto& binding : anim.animations)
        {
            if (binding.state == state)
                return &binding;
        }

        return nullptr;
    }

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
        auto* mesh = registry.GetComponent<MeshRendererComponent>(player);

        if (!movement || !anim || !mesh || !mesh->model)
            return;

        AnimationState desiredState = SelectMovementAnimation(*movement);
        anim->currentState = desiredState;
        anim->isPlaying = true;

        AnimationBinding* binding = FindBinding(*anim, desiredState);
        if (!binding)
            return;

        if (binding->index < 0)
            binding->index = mesh->model->FindAnimationIndexByName(binding->name);

        if (binding->index >= 0 && anim->animationIndex != binding->index)
        {
            anim->animationIndex = binding->index;
            anim->currentTime = 0.0f;
        }
    }

private:
    static AnimationState  SelectMovementAnimation(const CharacterMovement& movement)
    {
        if (!movement.isMoving)
            return AnimationState::Idle;

        if (movement.currentSpeed > movement.moveSpeed)
            return AnimationState::Run;

        return AnimationState::Walk;
    }
};

using AnimationController = CharacterAnimationControllerSystem;
