#pragma once
#include <ECSTypes.h>
struct OxygenGeneratorComponent
{
    

    // Скорости указаны в долях нормального состояния комнаты за секунду.
    float oxygenPerSecond = 0.05f;
    float pressurePerSecond = 0.035f;
    float temperatureRecoveryPerSecond = 1.5f;
    float targetTemperature = 20.0f;
    float powerUsage = 1.0f;

    bool enabled = true;
    bool hasPower = true;
    bool broken = false;
};
