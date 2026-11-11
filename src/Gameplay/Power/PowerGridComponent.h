#pragma once

struct PowerGridComponent
{
    float generationMw = 0.0f;
    float demandMw = 0.0f;
    float suppliedMw = 0.0f;
    float batteryOutputMw = 0.0f;
    float reserveMw = 0.0f;
    float loadRatio = 0.0f;

    // Небольшой накопитель даёт игроку время восстановить реактор после аварии.
    float batteryCapacityMwh = 0.25f;
    float batteryChargeMwh = 0.20f;
    float maxBatteryChargeMw = 3.0f;
    float maxBatteryDischargeMw = 4.0f;

    bool brownout = false;

    float BatteryRatio() const
    {
        return batteryCapacityMwh > 0.0f
            ? batteryChargeMwh / batteryCapacityMwh
            : 0.0f;
    }
};
