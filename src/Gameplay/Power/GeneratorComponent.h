#pragma once
struct GeneratorComponent
{
    float deuteriumKg = 0.0f;
    float tritiumKg = 0.0f;
    float maxDeuteriumKg = 500.0f;
    float maxTritiumKg = 500.0f;

    float maxOutputMw = 0.0f;
    float currentOutputMw = 0.0f;
    float targetOutputMw = 0.0f;
    float outputRampMwPerSecond = 4.0f;

    // Температуры указаны в градусах Цельсия.
    float heat = 25.0f;
    float ambientTemperature = 25.0f;
    float operatingTemperature = 680.0f;
    float scramTemperature = 950.0f;
    float restartTemperature = 320.0f;

    // Общий расход смеси D-T при полной мощности.
    float fuelBurnKgPerSecondAtMax = 0.025f;
    float currentFuelBurnKgPerSecond = 0.0f;

    float stability = 1.0f;
    bool isOnline = false;
    bool isScrammed = false;
};
