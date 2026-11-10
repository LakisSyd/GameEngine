#pragma once

struct RepairableComponent
{
    float progress = 0.0f;
    float requiredProgress = 1.0f;
    float repairRate = 0.35f;
    float maxRayDistance = 10.0f;
    bool repaired = false;

    float Progress01() const
    {
        return requiredProgress > 0.0f ? progress / requiredProgress : 1.0f;
    }
};

struct WorkZoneComponent
{
    Entity owner = INVALID_ENTITY;
};