#pragma once

#include <cmath>

#include <ECSRegistry.h>
#include "CharacterVitalsComponent.h"
#include "Components.h"

class PlayerVitalsSystem
{
public:
    static void Update(ECSRegistry& registry, float deltaTime)
    {
        if (!std::isfinite(deltaTime) || deltaTime <= 0.0f)
            return;

        registry.ForEach<CharacterVitalsComponent>(
            [&](Entity entity, CharacterVitalsComponent& vitals)
            {
                UpdateEntity(registry, entity, vitals, deltaTime);
            });
    }

private:
    static void UpdateEntity(
        ECSRegistry& registry,
        Entity entity,
        CharacterVitalsComponent& vitals,
        float deltaTime)
    {
        if (!registry.HasComponent<ActiveComponent>(entity))
            return;

        RoomComponent* currentRoom = nullptr;
        auto& rooms = registry.GetComponents<RoomComponent>();

        for (auto& [roomEntity, room] : rooms)
        {
            for (Entity object : room.objectsInRoom)
            {
                if (object == entity)
                {
                    currentRoom = &room;
                    break;
                }
            }

            if (currentRoom)
                break;
        }

        float targetOxygen = vitals.maxOxygen;
        if (currentRoom)
            targetOxygen = currentRoom->oxygen * vitals.maxOxygen;

        if (vitals.oxygen < targetOxygen)
        {
            vitals.oxygen += vitals.oxygenChangePerSecond * deltaTime;
            if (vitals.oxygen > targetOxygen)
                vitals.oxygen = targetOxygen;
        }
        else if (vitals.oxygen > targetOxygen)
        {
            vitals.oxygen -= vitals.oxygenChangePerSecond * deltaTime;
            if (vitals.oxygen < targetOxygen)
                vitals.oxygen = targetOxygen;
        }

        if (vitals.oxygen <= 0.0f &&
            vitals.takesSuffocationDamage &&
            !registry.HasComponent<InvulnerableComponent>(entity))
        {
            vitals.health -= vitals.suffocationDamagePerSecond * deltaTime;
        }

        StaminaUpdate(registry, entity, vitals, deltaTime);
        vitals.Normalize();
    }

    static void StaminaUpdate(
        ECSRegistry& registry,
        Entity entity,
        CharacterVitalsComponent& vitals,
        float deltaTime)
    {
        const auto* movement = registry.GetComponent<CharacterMovement>(entity);
        if (!movement)
            return;

        if (movement->isRunning)
            vitals.stamina -= vitals.staminaDrainPerSecond * deltaTime;
        else
            vitals.stamina += vitals.staminaRecoveryPerSecond * deltaTime;
    }
};
