#pragma once

#include <algorithm>
#include <unordered_map>
#include <vector>

#include <ECSRegistry.h>
#include <Components.h>

#include "EmergencyBatterySystem.h"
#include "GeneratorComponent.h"
#include "PowerConsumerComponent.h"
#include "PowerGridComponent.h"

class ShipPowerGridSystem
{
public:
    static void Update(ECSRegistry& registry, float deltaTime)
    {
        std::unordered_map<Entity, WorkingGrid> grids;

        // У каждого корабля своя сеть. Второй корабль больше не питает первый.
        registry.ForEach<PowerGridComponent>(
            [&](Entity root, PowerGridComponent& grid)
            {
                auto& working = grids[root];
                working.root = root;
                working.telemetry = &grid;
            });

        registry.ForEach<GeneratorComponent>(
            [&](Entity entity, GeneratorComponent& generator)
            {
                auto& grid = grids[FindGridRoot(registry, entity)];
                if (generator.isOnline && !generator.isScrammed)
                    grid.generationMw += (std::max)(0.0f, generator.currentOutputMw);
            });

        registry.ForEach<PowerConsumerComponent>(
            [&](Entity entity, PowerConsumerComponent& consumer)
            {
                consumer.suppliedMw = 0.0f;
                consumer.powerRatio = 0.0f;
                consumer.hasPower = false;

                auto& grid = grids[FindGridRoot(registry, entity)];
                grid.consumers.push_back(&consumer);
                grid.demandMw += (std::max)(0.0f, consumer.requiredMw);
            });

        for (auto& [root, grid] : grids)
        {
            (void)root;
            UpdateGrid(grid, deltaTime);
        }
    }

private:
    struct WorkingGrid
    {
        Entity root = INVALID_ENTITY;
        PowerGridComponent* telemetry = nullptr;
        std::vector<PowerConsumerComponent*> consumers;
        float generationMw = 0.0f;
        float demandMw = 0.0f;
    };

    static Entity FindGridRoot(ECSRegistry& registry, Entity entity)
    {
        if (registry.HasComponent<PowerGridComponent>(entity))
            return entity;

        if (const auto* part = registry.GetComponent<ShipPartComponent>(entity))
            return part->root;

        // Потребители вне корабля остаются в отдельной локальной сети.
        return INVALID_ENTITY;
    }

    static void UpdateGrid(WorkingGrid& grid, float deltaTime)
    {
        float availableMw = grid.generationMw;
        float batteryOutputMw = 0.0f;

        if (grid.telemetry && grid.demandMw > availableMw)
        {
            batteryOutputMw = EmergencyBatterySystem::Discharge(
                *grid.telemetry,
                grid.demandMw - availableMw,
                deltaTime);
            availableMw += batteryOutputMw;
        }

        for (int priorityIndex = 0; priorityIndex <= 3; ++priorityIndex)
        {
            const auto priority = static_cast<PowerPriority>(priorityIndex);
            std::vector<PowerConsumerComponent*> priorityConsumers;

            for (auto* consumer : grid.consumers)
            {
                if (consumer->priority == priority && consumer->requiredMw > 0.0f)
                    priorityConsumers.push_back(consumer);
            }

            SupplyEvenly(priorityConsumers, availableMw);
            if (availableMw <= 0.0001f)
                break;
        }

        float suppliedMw = 0.0f;
        for (auto* consumer : grid.consumers)
        {
            if (consumer->requiredMw <= 0.0f)
            {
                consumer->powerRatio = 1.0f;
                consumer->hasPower = true;
                continue;
            }

            consumer->powerRatio = std::clamp(
                consumer->suppliedMw / consumer->requiredMw,
                0.0f,
                1.0f);
            consumer->hasPower = consumer->powerRatio >= 0.999f;
            suppliedMw += consumer->suppliedMw;
        }

        const float generatorSurplusMw = (std::max)(0.0f, grid.generationMw - suppliedMw);
        if (grid.telemetry)
        {
            EmergencyBatterySystem::Charge(*grid.telemetry, generatorSurplusMw, deltaTime);

            grid.telemetry->generationMw = grid.generationMw;
            grid.telemetry->demandMw = grid.demandMw;
            grid.telemetry->suppliedMw = suppliedMw;
            grid.telemetry->batteryOutputMw = batteryOutputMw;
            grid.telemetry->reserveMw = generatorSurplusMw;
            grid.telemetry->loadRatio = grid.demandMw > 0.0f
                ? std::clamp(
                    grid.demandMw / (std::max)(
                        0.0001f,
                        grid.generationMw + batteryOutputMw),
                    0.0f,
                    1.0f)
                : 0.0f;
            grid.telemetry->brownout = suppliedMw + 0.0001f < grid.demandMw;
        }
    }

    static void SupplyEvenly(
        std::vector<PowerConsumerComponent*>& consumers,
        float& availableMw)
    {
        while (!consumers.empty() && availableMw > 0.0001f)
        {
            const float equalShare = availableMw / static_cast<float>(consumers.size());
            float usedMw = 0.0f;

            for (auto* consumer : consumers)
            {
                const float remainingNeed = (std::max)(
                    0.0f,
                    consumer->requiredMw - consumer->suppliedMw);
                const float supplied = (std::min)(remainingNeed, equalShare);
                consumer->suppliedMw += supplied;
                usedMw += supplied;
            }

            availableMw -= usedMw;
            consumers.erase(
                std::remove_if(
                    consumers.begin(),
                    consumers.end(),
                    [](const PowerConsumerComponent* consumer)
                    {
                        return consumer->suppliedMw >= consumer->requiredMw - 0.0001f;
                    }),
                consumers.end());

            if (usedMw <= 0.0001f)
                break;
        }
    }
};
