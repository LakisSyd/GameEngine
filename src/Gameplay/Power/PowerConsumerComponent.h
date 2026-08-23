#pragma once

#include <cstdint>

enum class PowerPriority : uint8_t
{
    Critical = 0, // аварийные системы, насосы, реактор
    High = 1, // важное оборудование
    Normal = 2, // обычные устройства
    Low = 3  // освещение, декоративные системы
};

struct PowerConsumerComponent
{

    

    float requiredMw = 0.0f;   // сколько нужно для полной работы
    float suppliedMw = 0.0f;   // сколько реально получил
    float powerRatio = 0.0f;   // 0..1

    PowerPriority priority = PowerPriority::Normal;

    bool hasPower = false;
};