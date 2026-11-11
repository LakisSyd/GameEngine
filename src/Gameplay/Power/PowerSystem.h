#pragma once

#include <algorithm>
#include <vector>

#include <Components.h>
#include <ECSRegistry.h>
#include "GeneratorComponent.h"
#include "PowerConsumerComponent.h"

class PowerSystem
{
public:
    void Update(ECSRegistry& registry, float delta)
    {
        (void)delta;

        float availableMw = 0.0f;

        // 1. Генерация
        registry.ForEach<GeneratorComponent>(
            [&](Entity entity, GeneratorComponent& generator)
            {
                if (!registry.HasComponent<ActiveComponent>(entity) ||
                    !generator.isOnline)
                    return;

                availableMw += (std::max)(0.0f, generator.currentOutputMw);
            });

        // 2. Сбрасываем питание прошлого кадра
        registry.ForEach<PowerConsumerComponent>(
            [&](Entity, PowerConsumerComponent& consumer)
            {
                consumer.suppliedMw = 0.0f;
                consumer.powerRatio = 0.0f;
                consumer.hasPower = false;
            });

        // 3. Подаём питание по приоритетам
        for (int priorityIndex = 0; priorityIndex <= 3; ++priorityIndex)
        {
            const auto priority =
                static_cast<PowerPriority>(priorityIndex);

            std::vector<PowerConsumerComponent*> consumers;

            registry.ForEach<PowerConsumerComponent>(
                [&](Entity entity, PowerConsumerComponent& consumer)
                {
                    if (!registry.HasComponent<ActiveComponent>(entity) ||
                        consumer.priority != priority)
                        return;

                    if (consumer.requiredMw <= 0.0f)
                        return;

                    consumers.push_back(&consumer);
                });

            SupplyEvenly(consumers, availableMw);

            if (availableMw <= 0.0001f)
                break;
        }

        // 4. Итоговое состояние устройств
        registry.ForEach<PowerConsumerComponent>(
            [&](Entity entity, PowerConsumerComponent& consumer)
            {
                // The reset pass above deliberately leaves inactive consumers
                // at zero even when they require no power.
                if (!registry.HasComponent<ActiveComponent>(entity))
                    return;

                if (consumer.requiredMw <= 0.0f)
                {
                    consumer.powerRatio = 1.0f;
                    consumer.hasPower = true;
                    return;
                }

                consumer.powerRatio =
                    std::clamp(
                        consumer.suppliedMw / consumer.requiredMw,
                        0.0f,
                        1.0f);

                // Для дверей, насосов и т.п. питание считается рабочим,
                // только когда устройство получило всю требуемую мощность.
                consumer.hasPower = consumer.powerRatio >= 0.999f;
            });
    }

private:
    static void SupplyEvenly(
        std::vector<PowerConsumerComponent*>& consumers,
        float& availableMw)
    {
        while (!consumers.empty() && availableMw > 0.0001f)
        {
            const float equalShare = availableMw /
                static_cast<float>(consumers.size());

            float usedMw = 0.0f;

            for (auto* consumer : consumers)
            {
                const float remainingNeed = (std::max)(
                    0.0f,
                    consumer->requiredMw - consumer->suppliedMw);

                const float supplied = std::min(
                    remainingNeed,
                    equalShare);

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
                        return consumer->suppliedMw >=
                            consumer->requiredMw - 0.0001f;
                    }),
                consumers.end());

            if (usedMw <= 0.0001f)
                break;
        }
    }
};
