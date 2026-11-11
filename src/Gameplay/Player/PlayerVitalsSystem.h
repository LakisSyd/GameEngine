#pragma once

#include <algorithm>

#include <ECSRegistry.h>
#include "PlayerComponents.h"
#include "Components.h"

class PlayerVitalsSystem
{
public:
    static void Update(ECSRegistry& registry, Entity player, float deltaTime)
    {
        if (player == INVALID_ENTITY)
            return;

        if (deltaTime <= 0.0f)
            return;

        auto* playerComp = registry.GetComponent<PlayerComponent>(player);
        if (!playerComp)
            return;

        RoomComponent* currentRoom = nullptr;

        auto& rooms = registry.GetComponents<RoomComponent>();

        for (auto& [roomEntity, room] : rooms)
        {
            for (Entity object : room.objectsInRoom)
            {
                if (object == player)
                {
                    currentRoom = &room;
                    break;
                }
            }

            if (currentRoom)
                break;
        }

        float targetOxygen = 100.0f;

        if (currentRoom)
        {
            targetOxygen = currentRoom->oxygen * 100.0f;
        }

        const float oxygenChangeSpeed = 25.0f;
        const float damagePerSecond = 10.0f;

        if (playerComp->oxygen < targetOxygen)
        {
            playerComp->oxygen += oxygenChangeSpeed * deltaTime;

            if (playerComp->oxygen > targetOxygen)
                playerComp->oxygen = targetOxygen;
        }
        else if (playerComp->oxygen > targetOxygen)
        {
            playerComp->oxygen -= oxygenChangeSpeed * deltaTime;

            if (playerComp->oxygen < targetOxygen)
                playerComp->oxygen = targetOxygen;
        }

        playerComp->oxygen = std::clamp(playerComp->oxygen, 0.0f, 100.0f);

        if (playerComp->oxygen <= 0.0f)
        {
            playerComp->health -= damagePerSecond * deltaTime;
        }

        playerComp->health = std::clamp(playerComp->health, 0.0f, 100.0f);

		StaminaUpdate(registry, player, deltaTime);

    }

   static void StaminaUpdate(ECSRegistry& registry, Entity player, float deltaTime)
    {


        if (player == INVALID_ENTITY)
            return;

		auto* playerComp = registry.GetComponent<PlayerComponent>(player);
		auto* movementComp = registry.GetComponent<CharacterMovement>(player);
        if(!playerComp)
			return;

        if (!movementComp)
            return;

        if (movementComp->isRunning)
        {
            playerComp->stamina -= 20.0f * deltaTime;
        }
        else if (!movementComp->wantsRun)
        {
            playerComp->stamina += 10.0f * deltaTime;
        }
        playerComp->Normalize();
    }
};