#pragma once

#include "EditorUI.h"
#include "../../CharacterMovementSystem.h"
#include "../../CharacterSpawnSystem.h"

#include <windows.h>

enum class EngineMode
{
    Editor,
    Play
};

class GamePlayController
{
public:
    void Update(
        ECSRegistry& registry,
        float dt,
        InputClass* input,
        bool gameplayBlocked,
        float cameraYaw)
    {
        if (gameplayBlocked)
            return;

        CharacterMovementSystem::Update(registry, dt, input, cameraYaw);

        // Future gameplay systems:
        // InteractionSystem::Update(...);
        // MissionDirector::Update(...);
        // SpellSystem::Update(...);
    }

    bool EnterPlayMode(ECSRegistry& registry)
    {
        Entity player = INVALID_ENTITY;
        int playerCount = 0;

        registry.ForEach<PlayerGameTag>([&](Entity entity, PlayerGameTag&)
        {
            player = entity;
            ++playerCount;
        });

        if (player == INVALID_ENTITY)
        {
            OutputDebugStringA("[Gameplay] EnterPlayMode failed: no entity with PlayerGameTag found.\n");
            return false;
        }

        if (playerCount > 1)
        {
            OutputDebugStringA("[Gameplay] Warning: multiple PlayerGameTag entities found; using the last one.\n");
        }

        auto* transform = registry.GetComponent<TransformComponent>(player);
        if (!transform)
        {
            OutputDebugStringA("[Gameplay] EnterPlayMode failed: player has no TransformComponent.\n");
            return false;
        }

		auto* anim = registry.GetComponent<AnimationComponent>(player);
        if(!anim)
        {
            registry.AddComponent<AnimationComponent>(player, AnimationComponent{});
            anim = registry.GetComponent<AnimationComponent>(player);
            OutputDebugStringA("[Gameplay] Added AnimationComponent to player.\n");
		}

        auto* movement = registry.GetComponent<CharacterMovement>(player);
        if (!movement)
        {
            registry.AddComponent<CharacterMovement>(player, CharacterMovement{});
            movement = registry.GetComponent<CharacterMovement>(player);
            OutputDebugStringA("[Gameplay] Added CharacterMovement to player.\n");
        }

        if (!movement)
        {
            OutputDebugStringA("[Gameplay] EnterPlayMode failed: CharacterMovement was not created.\n");
            return false;
        }

        auto* controller = registry.GetComponent<CharacterController>(player);
        if (!controller)
        {
            registry.AddComponent<CharacterController>(player, CharacterController{});
            controller = registry.GetComponent<CharacterController>(player);
            OutputDebugStringA("[Gameplay] Added CharacterController to player.\n");
        }

        if (!controller)
        {
            OutputDebugStringA("[Gameplay] EnterPlayMode failed: CharacterController was not created.\n");
            return false;
        }

        const bool controllerReady =
            controller->controller != nullptr &&
            controller->ghostObj != nullptr &&
            controller->capsuleShape != nullptr;

        const bool controllerEmpty =
            controller->controller == nullptr &&
            controller->ghostObj == nullptr &&
            controller->capsuleShape == nullptr;

        if (!controllerReady)
        {
            if (!controllerEmpty)
            {
                OutputDebugStringA("[Gameplay] EnterPlayMode failed: CharacterController is partially initialized.\n");
                return false;
            }

            CharacterSpawnSystem::Spawn(registry, player, transform->position);
            OutputDebugStringA("[Gameplay] Spawned Bullet character controller for player.\n");
        }

        return true;
    }
};
