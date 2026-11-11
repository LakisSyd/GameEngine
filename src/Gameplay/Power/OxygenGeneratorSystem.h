#pragma once

#include <algorithm>
#include <cmath>

#include <ECSRegistry.h>
#include <Components.h>

#include "OxygenGeneratorComponent.h"

class OxygenGeneratorSystem
{
public:
    static void Update(ECSRegistry& registry, float deltaTime)
    {
        if (deltaTime <= 0.0f)
            return;

        float totalOxygenPerSecond = 0.0f;
        float totalPressurePerSecond = 0.0f;
        float totalTemperatureRecoveryPerSecond = 0.0f;
        float weightedTargetTemperature = 0.0f;

        registry.ForEach<OxygenGeneratorComponent>(
            [&](Entity entity, OxygenGeneratorComponent& generator)
            {
                if (!registry.HasComponent<ActiveComponent>(entity) ||
                    !generator.enabled || !generator.hasPower || generator.broken)
                    return;

                totalOxygenPerSecond += generator.oxygenPerSecond;
                totalPressurePerSecond += generator.pressurePerSecond;
                totalTemperatureRecoveryPerSecond += generator.temperatureRecoveryPerSecond;
                weightedTargetTemperature +=
                    generator.targetTemperature * generator.temperatureRecoveryPerSecond;
            });

        if (totalOxygenPerSecond <= 0.0f &&
            totalPressurePerSecond <= 0.0f &&
            totalTemperatureRecoveryPerSecond <= 0.0f)
            return;

        int activeRoomCount = 0;

        registry.ForEach<RoomComponent>(
            [&](Entity roomEntity, RoomComponent& room)
            {
                const bool needsOxygen = room.oxygen < 1.0f;
                const bool needsPressure = room.pressure < 1.0f;
                const float targetTemperature = totalTemperatureRecoveryPerSecond > 0.0f
                    ? weightedTargetTemperature / totalTemperatureRecoveryPerSecond
                    : room.temperature;
                const bool needsTemperature =
                    std::abs(room.temperature - targetTemperature) > 0.01f;

                if (!registry.HasComponent<ActiveComponent>(roomEntity) ||
                    !room.isActive ||
                    (!needsOxygen && !needsPressure && !needsTemperature))
                    return;

                ++activeRoomCount;
            });

        if (activeRoomCount <= 0)
            return;

        const float oxygenPerRoom = totalOxygenPerSecond / static_cast<float>(activeRoomCount);
        const float pressurePerRoom = totalPressurePerSecond / static_cast<float>(activeRoomCount);
        const float temperatureRecoveryPerRoom =
            totalTemperatureRecoveryPerSecond / static_cast<float>(activeRoomCount);
        const float targetTemperature = totalTemperatureRecoveryPerSecond > 0.0f
            ? weightedTargetTemperature / totalTemperatureRecoveryPerSecond
            : 20.0f;

        registry.ForEach<RoomComponent>(
            [&](Entity roomEntity, RoomComponent& room)
            {
                if (!registry.HasComponent<ActiveComponent>(roomEntity) ||
                    !room.isActive)
                    return;

                room.oxygen += oxygenPerRoom * deltaTime;
                room.pressure += pressurePerRoom * deltaTime;
                room.temperature += std::clamp(
                    targetTemperature - room.temperature,
                    -temperatureRecoveryPerRoom * deltaTime,
                    temperatureRecoveryPerRoom * deltaTime);

                room.oxygen = std::clamp(room.oxygen, 0.0f, 1.0f);
                room.pressure = std::clamp(room.pressure, 0.0f, 1.0f);
            });
    }
};
