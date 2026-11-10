#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include <ECSRegistry.h>
#include "Components.h"
#include "DecompressionSourceComponent.h"
#include "Gameplay/Player/CharacterExternalVelocitySystem.h"

class RoomEnvironmentSystem
{
public:
    static void Update(ECSRegistry& registry, float deltaTime)
    {
        if (deltaTime <= 0.0f)
            return;

        deltaTime = (std::min)(deltaTime, 0.10f);
        auto& rooms = registry.GetComponents<RoomComponent>();

        CharacterExternalVelocitySystem::BeginAtmosphereFrame();

        for (auto& [roomEntity, room] : rooms)
        {
            (void)roomEntity;
            room.leakRate = 0.0f;
            if (!room.isActive)
                continue;

            int breathingEntities = 0;
            for (Entity entity : room.objectsInRoom)
            {
                if (registry.HasComponent<CharacterController>(entity))
                    ++breathingEntities;
            }

            room.oxygen -= static_cast<float>(breathingEntities) * 0.01f * deltaTime;
            room.oxygen = std::clamp(room.oxygen, 0.0f, 1.0f);
        }

        ApplyBreaches(registry, deltaTime);
        EqualizeConnectedRooms(registry, deltaTime);
    }

private:
    struct RoomTransfer
    {
        Entity roomA = INVALID_ENTITY;
        Entity roomB = INVALID_ENTITY;
        float oxygen = 0.0f;
        float pressure = 0.0f;
        float temperature = 0.0f;
    };

    static void ApplyBreaches(ECSRegistry& registry, float deltaTime)
    {
        auto& breaches = registry.GetComponents<HullBreachComponent>();
        for (auto& [breachEntity, breach] : breaches)
        {
            if (breach.sealed || breach.room == INVALID_ENTITY)
                continue;

            auto* room = registry.GetComponent<RoomComponent>(breach.room);
            if (!room || !room->isActive)
                continue;

            // Старые пробоины из сохранений автоматически получают новые настройки.
            if (!registry.HasComponent<DecompressionSourceComponent>(breachEntity))
            {
                registry.AddComponent<DecompressionSourceComponent>(
                    breachEntity,
                    DecompressionSourceComponent{});
            }

            auto* source = registry.GetComponent<DecompressionSourceComponent>(breachEntity);
            if (!source)
                continue;

            const float radiusScale = std::clamp(breach.radius / 0.25f, 0.25f, 4.0f);
            const float effectiveLeak = (std::max)(0.0f, breach.leakRate) * radiusScale;
            const float pressureBefore = std::clamp(room->pressure, 0.0f, 1.0f);
            const float lostFraction = std::clamp(effectiveLeak * deltaTime, 0.0f, 0.95f);

            room->leakRate += effectiveLeak;
            room->pressure *= 1.0f - lostFraction;
            room->oxygen *= 1.0f - lostFraction;

            // При резком падении давления газ охлаждается. Пока пробоина открыта,
            // остаётся небольшая дополнительная тяга к внешней температуре.
            const float thermalStep = std::clamp(
                source->coolingRate * (lostFraction + effectiveLeak * 0.012f * deltaTime),
                0.0f,
                1.0f);
            room->temperature +=
                (source->outsideTemperature - room->temperature) * thermalStep;

            room->pressure = std::clamp(room->pressure, 0.0f, 1.0f);
            room->oxygen = std::clamp(room->oxygen, 0.0f, 1.0f);
            room->temperature = std::clamp(room->temperature, -150.0f, 80.0f);

            ApplyPlayerSuction(registry, breach, *source, *room, pressureBefore);
        }
    }

    static void ApplyPlayerSuction(
        ECSRegistry& registry,
        const HullBreachComponent& breach,
        const DecompressionSourceComponent& source,
        const RoomComponent& room,
        float roomPressure)
    {
        const float suctionRadius = (std::max)(0.0f, source.suctionRadius);
        if (suctionRadius <= 0.0f || source.suctionStrength <= 0.0f || roomPressure <= 0.001f)
            return;

        for (Entity entity : room.objectsInRoom)
        {
            // Пока воздействуем только на игрока, как и требуется. Предметы можно
            // будет подключить позднее той же функцией через RigidBodyComponent.
            if (!registry.HasComponent<PlayerGameTag>(entity))
                continue;

            auto* transform = registry.GetComponent<TransformComponent>(entity);
            if (!transform)
                continue;

            DirectX::XMFLOAT3 direction = {
                breach.worldPosition.x - transform->position.x,
                breach.worldPosition.y - transform->position.y,
                breach.worldPosition.z - transform->position.z
            };
            const float distanceSquared =
                direction.x * direction.x +
                direction.y * direction.y +
                direction.z * direction.z;
            if (distanceSquared <= 0.000001f || distanceSquared >= suctionRadius * suctionRadius)
                continue;

            const float distance = std::sqrt(distanceSquared);
            direction.x /= distance;
            direction.y /= distance;
            direction.z /= distance;

            const float distanceFalloff = 1.0f - distance / suctionRadius;
            const float speed = source.suctionStrength * roomPressure *
                distanceFalloff * distanceFalloff;
            const DirectX::XMFLOAT3 velocity = {
                direction.x * speed,
                direction.y * speed,
                direction.z * speed
            };

            CharacterExternalVelocitySystem::AddVelocity(
                entity,
                velocity,
                source.maxSuctionSpeed);
        }
    }

    static void EqualizeConnectedRooms(ECSRegistry& registry, float deltaTime)
    {
        constexpr float oxygenFlowRate = 0.35f;
        constexpr float pressureFlowRate = 0.55f;
        constexpr float temperatureFlowRate = 0.14f;

        auto& rooms = registry.GetComponents<RoomComponent>();
        std::vector<RoomTransfer> transfers;

        for (auto& [roomEntity, room] : rooms)
        {
            if (!room.isActive)
                continue;

            for (Entity connectedRoomEntity : room.connectedRooms)
            {
                if (roomEntity >= connectedRoomEntity)
                    continue;

                auto* connectedRoom = registry.GetComponent<RoomComponent>(connectedRoomEntity);
                if (!connectedRoom || !connectedRoom->isActive)
                    continue;

                const float doorFlow = GetDoorFlowMultiplier(
                    registry,
                    roomEntity,
                    connectedRoomEntity);
                if (doorFlow <= 0.0f)
                    continue;

                RoomTransfer transfer;
                transfer.roomA = roomEntity;
                transfer.roomB = connectedRoomEntity;
                transfer.oxygen = LimitedFlow(
                    room.oxygen - connectedRoom->oxygen,
                    oxygenFlowRate * doorFlow * deltaTime);
                transfer.pressure = LimitedFlow(
                    room.pressure - connectedRoom->pressure,
                    pressureFlowRate * doorFlow * deltaTime);
                transfer.temperature = LimitedFlow(
                    room.temperature - connectedRoom->temperature,
                    temperatureFlowRate * doorFlow * deltaTime);
                transfers.push_back(transfer);
            }
        }

        for (const RoomTransfer& transfer : transfers)
        {
            auto* roomA = registry.GetComponent<RoomComponent>(transfer.roomA);
            auto* roomB = registry.GetComponent<RoomComponent>(transfer.roomB);
            if (!roomA || !roomB)
                continue;

            roomA->oxygen -= transfer.oxygen;
            roomB->oxygen += transfer.oxygen;
            roomA->pressure -= transfer.pressure;
            roomB->pressure += transfer.pressure;
            roomA->temperature -= transfer.temperature;
            roomB->temperature += transfer.temperature;

            roomA->oxygen = std::clamp(roomA->oxygen, 0.0f, 1.0f);
            roomB->oxygen = std::clamp(roomB->oxygen, 0.0f, 1.0f);
            roomA->pressure = std::clamp(roomA->pressure, 0.0f, 1.0f);
            roomB->pressure = std::clamp(roomB->pressure, 0.0f, 1.0f);
        }
    }

    static float LimitedFlow(float difference, float rateForFrame)
    {
        const float flow = difference * rateForFrame;
        const float limit = std::fabs(difference) * 0.5f;
        return std::clamp(flow, -limit, limit);
    }

    static float GetDoorFlowMultiplier(ECSRegistry& registry, Entity roomA, Entity roomB)
    {
        bool hasDoorBetweenRooms = false;
        float bestFlow = 0.0f;

        auto& doors = registry.GetComponents<DoorComponent>();
        for (auto& [doorEntity, door] : doors)
        {
            (void)doorEntity;
            const bool matchesForward = door.roomA == roomA && door.roomB == roomB;
            const bool matchesBackward = door.roomA == roomB && door.roomB == roomA;
            if (!matchesForward && !matchesBackward)
                continue;

            hasDoorBetweenRooms = true;
            bestFlow = (std::max)(bestFlow, door.GetEffectiveFlowRate());
        }

        return hasDoorBetweenRooms ? bestFlow : 1.0f;
    }
};
