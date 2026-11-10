#pragma once

#include <algorithm>
#include <cmath>

#include <ECSRegistry.h>
#include "GeneratorComponent.h"

class GeneratorSystem
{
public:
    static void Update(ECSRegistry& registry, float deltaTime)
    {
        if (deltaTime <= 0.0f)
            return;

        registry.ForEach<GeneratorComponent>(
            [&](Entity, GeneratorComponent& generator)
            {
                UpdateGenerator(generator, deltaTime);
            });
    }

private:
    static float MoveTowards(float current, float target, float maxDelta)
    {
        if (current < target)
            return (std::min)(current + maxDelta, target);
        return (std::max)(current - maxDelta, target);
    }

    static void UpdateGenerator(GeneratorComponent& generator, float deltaTime)
    {
        generator.maxOutputMw = (std::max)(0.0f, generator.maxOutputMw);
        generator.targetOutputMw = std::clamp(
            generator.targetOutputMw,
            0.0f,
            generator.maxOutputMw);
        generator.deuteriumKg = std::clamp(
            generator.deuteriumKg,
            0.0f,
            (std::max)(0.0f, generator.maxDeuteriumKg));
        generator.tritiumKg = std::clamp(
            generator.tritiumKg,
            0.0f,
            (std::max)(0.0f, generator.maxTritiumKg));

        const bool hasFuel = generator.deuteriumKg > 0.0001f &&
            generator.tritiumKg > 0.0001f;

        // SCRAM аварийно гасит плазму. Повторный запуск разрешён после охлаждения.
        if (generator.heat >= generator.scramTemperature)
        {
            generator.isOnline = false;
            generator.isScrammed = true;
        }
        else if (generator.isScrammed && generator.heat <= generator.restartTemperature)
        {
            generator.isScrammed = false;
        }

        if (!hasFuel)
            generator.isOnline = false;

        const float effectiveTarget = generator.isOnline && !generator.isScrammed && hasFuel
            ? generator.targetOutputMw
            : 0.0f;
        generator.currentOutputMw = MoveTowards(
            generator.currentOutputMw,
            effectiveTarget,
            (std::max)(0.0f, generator.outputRampMwPerSecond) * deltaTime);

        const float outputRatio = generator.maxOutputMw > 0.0f
            ? std::clamp(generator.currentOutputMw / generator.maxOutputMw, 0.0f, 1.0f)
            : 0.0f;

        generator.currentFuelBurnKgPerSecond = generator.isOnline
            ? generator.fuelBurnKgPerSecondAtMax * outputRatio
            : 0.0f;

        const float fuelBurn = generator.currentFuelBurnKgPerSecond * deltaTime;
        generator.deuteriumKg = (std::max)(0.0f, generator.deuteriumKg - fuelBurn * 0.5f);
        generator.tritiumKg = (std::max)(0.0f, generator.tritiumKg - fuelBurn * 0.5f);

        // Температура плавно стремится к рабочей точке и остывает после отключения.
        const float targetTemperature = generator.isOnline
            ? generator.ambientTemperature +
                (generator.operatingTemperature - generator.ambientTemperature) * outputRatio
            : generator.ambientTemperature;
        const float thermalResponse = generator.isOnline ? 0.10f : 0.16f;
        const float thermalBlend = 1.0f - std::exp(-thermalResponse * deltaTime);
        generator.heat += (targetTemperature - generator.heat) * thermalBlend;
        generator.heat = (std::max)(generator.ambientTemperature, generator.heat);

        const float dangerRange = (std::max)(
            1.0f,
            generator.scramTemperature - generator.operatingTemperature);
        const float thermalPenalty = std::clamp(
            (generator.heat - generator.operatingTemperature) / dangerRange,
            0.0f,
            1.0f);
        generator.stability = std::clamp(1.0f - thermalPenalty * 0.85f, 0.0f, 1.0f);
    }
};
