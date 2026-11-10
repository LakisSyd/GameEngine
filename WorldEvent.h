#pragma once
#include "ECSRegistry.h"
enum class WorldEventType
{
    // EquipmentFailed means the device stopped working but still exists.
    EquipmentFailed,
    FireStarted,
    FireExtinguished,
    BreachOpened,
    BreachSealed,
    PowerBrownout,
    EquipmentRepaired,
    EquipmentDestroyed
};

struct WorldEvent
{
    WorldEventType type{};

    double timeSeconds = 0.0;

    Entity source = INVALID_ENTITY;
    Entity subject = INVALID_ENTITY;
    Entity room = INVALID_ENTITY;

    float severity = 0.0f;

    
};
