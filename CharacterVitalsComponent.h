#pragma once

#include <algorithm>

// Общие изменяемые характеристики персонажа. Значения измеряются в очках,
// скорости изменения — в очках в секунду.
struct CharacterVitalsComponent
{
    float health = 100.0f;
    float maxHealth = 100.0f;

    float oxygen = 100.0f;
    float maxOxygen = 100.0f;
    float oxygenChangePerSecond = 25.0f;
    float suffocationDamagePerSecond = 10.0f;

    float stamina = 100.0f;
    float maxStamina = 100.0f;
    float staminaDrainPerSecond = 20.0f;
    float staminaRecoveryPerSecond = 10.0f;
    float sprintStartThreshold = 5.0f;
    // Runtime-состояние: после полного истощения бег возобновляется только
    // после восстановления до sprintStartThreshold.
    bool staminaExhausted = false;

	bool takesSuffocationDamage = true;

    void Normalize()
    {
        maxHealth = (std::max)(0.0f, maxHealth);
        maxOxygen = (std::max)(0.0f, maxOxygen);
        maxStamina = (std::max)(0.0f, maxStamina);

        health = std::clamp(health, 0.0f, maxHealth);
        oxygen = std::clamp(oxygen, 0.0f, maxOxygen);
        stamina = std::clamp(stamina, 0.0f, maxStamina);
        if (stamina <= 0.0f)
            staminaExhausted = true;

        oxygenChangePerSecond = (std::max)(0.0f, oxygenChangePerSecond);
        suffocationDamagePerSecond = (std::max)(0.0f, suffocationDamagePerSecond);
        staminaDrainPerSecond = (std::max)(0.0f, staminaDrainPerSecond);
        staminaRecoveryPerSecond = (std::max)(0.0f, staminaRecoveryPerSecond);
        sprintStartThreshold = std::clamp(sprintStartThreshold, 0.0f, maxStamina);
    }
};
