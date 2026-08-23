#pragma once

#include <ECSRegistry.h>
#include <Components.h>

#include "Gameplay/Power/GeneratorComponent.h"
#include "Gameplay/Power/PowerGridComponent.h"

enum class ShipAlertLevel
{
    None = 0,
    Advisory = 1,
    Warning = 2,
    Critical = 3
};

struct ShipAlertSnapshot
{
    ShipAlertLevel level = ShipAlertLevel::None;
    const wchar_t* message = L"";
};

class ShipAlertSystem
{
public:
    static void Update(ECSRegistry& registry, Entity player)
    {
        s_current = {};

        Entity playerRoom = INVALID_ENTITY;
        registry.ForEach<RoomComponent>(
            [&](Entity roomEntity, RoomComponent& room)
            {
                if (playerRoom != INVALID_ENTITY)
                    return;

                for (Entity object : room.objectsInRoom)
                {
                    if (object == player)
                    {
                        playerRoom = roomEntity;
                        break;
                    }
                }
            });

        if (playerRoom != INVALID_ENTITY)
        {
            if (const auto* room = registry.GetComponent<RoomComponent>(playerRoom))
            {
                if (room->oxygen <= 0.20f)
                    SetAlert(ShipAlertLevel::Critical, L"ATMOSPHERE CRITICAL");
                else if (room->oxygen <= 0.45f)
                    SetAlert(ShipAlertLevel::Warning, L"LOW ATMOSPHERE");
            }

            for (auto& [entity, breach] : registry.GetComponents<HullBreachComponent>())
            {
                (void)entity;
                if (!breach.sealed && breach.room == playerRoom)
                {
                    SetAlert(ShipAlertLevel::Critical, L"HULL BREACH DETECTED");
                    break;
                }
            }
        }

        Entity shipRoot = INVALID_ENTITY;
        if (const auto* roomPart = registry.GetComponent<ShipPartComponent>(playerRoom))
            shipRoot = roomPart->root;

        if (const auto* grid = registry.GetComponent<PowerGridComponent>(shipRoot))
        {
            if (grid->brownout)
                SetAlert(ShipAlertLevel::Critical, L"POWER GRID BROWNOUT");
            else if (grid->generationMw <= 0.001f && grid->batteryOutputMw > 0.001f)
                SetAlert(ShipAlertLevel::Warning, L"EMERGENCY POWER");
            else if (grid->BatteryRatio() <= 0.15f)
                SetAlert(ShipAlertLevel::Advisory, L"BATTERY RESERVE LOW");
        }

        registry.ForEach<GeneratorComponent>(
            [&](Entity entity, GeneratorComponent& generator)
            {
                const auto* part = registry.GetComponent<ShipPartComponent>(entity);
                if (shipRoot != INVALID_ENTITY && (!part || part->root != shipRoot))
                    return;

                if (generator.isScrammed)
                    SetAlert(ShipAlertLevel::Critical, L"REACTOR SCRAM");
                else if (generator.heat >= generator.operatingTemperature * 1.15f)
                    SetAlert(ShipAlertLevel::Warning, L"REACTOR TEMPERATURE HIGH");
                else if (generator.deuteriumKg <= generator.maxDeuteriumKg * 0.10f ||
                    generator.tritiumKg <= generator.maxTritiumKg * 0.10f)
                {
                    SetAlert(ShipAlertLevel::Advisory, L"FUSION FUEL LOW");
                }
            });
    }

    static const ShipAlertSnapshot& GetCurrentAlert()
    {
        return s_current;
    }

private:
    static void SetAlert(ShipAlertLevel level, const wchar_t* message)
    {
        if (static_cast<int>(level) <= static_cast<int>(s_current.level))
            return;

        s_current.level = level;
        s_current.message = message;
    }

    inline static ShipAlertSnapshot s_current{};
};
