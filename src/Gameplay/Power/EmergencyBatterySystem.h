#pragma once

#include <algorithm>
#include "PowerGridComponent.h"

class EmergencyBatterySystem
{
public:
    static float Discharge(PowerGridComponent& grid, float requestedMw, float deltaTime)
    {
        if (requestedMw <= 0.0f || deltaTime <= 0.0f || grid.batteryChargeMwh <= 0.0f)
            return 0.0f;

        const float energyLimitedMw = grid.batteryChargeMwh * 3600.0f / deltaTime;
        const float suppliedMw = (std::min)({
            requestedMw,
            grid.maxBatteryDischargeMw,
            energyLimitedMw });

        grid.batteryChargeMwh -= suppliedMw * deltaTime / 3600.0f;
        grid.batteryChargeMwh = (std::max)(0.0f, grid.batteryChargeMwh);
        return suppliedMw;
    }

    static float Charge(PowerGridComponent& grid, float surplusMw, float deltaTime)
    {
        if (surplusMw <= 0.0f || deltaTime <= 0.0f ||
            grid.batteryChargeMwh >= grid.batteryCapacityMwh)
        {
            return 0.0f;
        }

        const float capacityLimitedMw =
            (grid.batteryCapacityMwh - grid.batteryChargeMwh) * 3600.0f / deltaTime;
        const float acceptedMw = (std::min)({
            surplusMw,
            grid.maxBatteryChargeMw,
            capacityLimitedMw });

        grid.batteryChargeMwh += acceptedMw * deltaTime / 3600.0f;
        grid.batteryChargeMwh = std::clamp(
            grid.batteryChargeMwh,
            0.0f,
            grid.batteryCapacityMwh);
        return acceptedMw;
    }
};
