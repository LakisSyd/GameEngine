#pragma once

#include "AnimationComponent.h"
#include "Components.h"
#include "ECSRegistry.h"
#include "ECSTypes.h"
#include "Gameplay/Combat/WeaponComponent.h"

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

        // Animation is locomotion-driven for every character, not only the
        // PlayerGameTag entity. This automatically covers NPC walk/run commands.
        registry.ForEach<AnimationComponent, CharacterMovement>(
            [&](Entity entity, AnimationComponent& anim, CharacterMovement& movement)
            {
                auto* mesh = registry.GetComponent<MeshRendererComponent>(entity);
                if (!mesh || !mesh->model)
                    return;

                AnimationState desiredState = SelectMovementAnimation(movement);
                if (auto* equipped = registry.GetComponent<EquippedWeaponComponent>(entity))
                {
                    if (auto* weapon = registry.GetComponent<WeaponComponent>(equipped->weapon))
                    {
                        SetBindingName(anim, AnimationState::RifleIdle, weapon->rifleIdleAnimation);
                        SetBindingName(anim, AnimationState::RifleGun, weapon->rifleGunAnimation);
                        desiredState = weapon->triggerHeld
                            ? AnimationState::RifleGun
                            : AnimationState::RifleIdle;
                    }
                }
                anim.currentState = desiredState;

                AnimationBinding* binding = FindBinding(anim, desiredState);
                if (!binding)
                {
                    anim.isPlaying = false;
                    return;
                }

                if (binding->index < 0)
                    binding->index = mesh->model->FindAnimationIndexByName(binding->name);

                // Characters may currently have locomotion clips only. Walk and
                // Run can safely fall back to one another; without an Idle clip,
                // pause instead of continuing to walk in place forever.
                if (binding->index < 0 && desiredState != AnimationState::Idle)
                {
                    AnimationState fallbackState = AnimationState::Idle;
                    if (desiredState == AnimationState::Run)
                        fallbackState = AnimationState::Walk;
                    else if (desiredState == AnimationState::Walk)
                        fallbackState = AnimationState::Run;
                    else if (desiredState == AnimationState::RifleIdle)
                        fallbackState = AnimationState::RifleGun;
                    else if (desiredState == AnimationState::RifleGun)
                        fallbackState = AnimationState::RifleIdle;
                    AnimationBinding* fallback = FindBinding(anim, fallbackState);
                    if (fallback)
                    {
                        if (fallback->index < 0)
                            fallback->index = mesh->model->FindAnimationIndexByName(fallback->name);
                        if (fallback->index >= 0)
                            binding = fallback;
                    }
                }

                if (binding->index < 0)
                {
                    anim.isPlaying = false;
                    return;
                }

                anim.isPlaying = true;

                if (anim.animationIndex != binding->index)
                {
                    anim.animationIndex = binding->index;
                    anim.currentTime = 0.0f;
                }
            });
    }

private:
    static void SetBindingName(
        AnimationComponent& animation,
        AnimationState state,
        const std::string& name)
    {
        if (AnimationBinding* binding = FindBinding(animation, state))
        {
            if (binding->name != name)
            {
                binding->name = name;
                binding->index = -1;
            }
        }
    }

    static AnimationState  SelectMovementAnimation(const CharacterMovement& movement)
    {
        if (movement.locomotionState == PlayerLocomotionState::Idle)
            return AnimationState::Idle;

        if (movement.locomotionState == PlayerLocomotionState::Run)
            return AnimationState::Run;

        return AnimationState::Walk;
    }
};

using AnimationController = CharacterAnimationControllerSystem;
