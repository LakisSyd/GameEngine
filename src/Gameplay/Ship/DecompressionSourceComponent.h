#pragma once

struct DecompressionSourceComponent
{
    // Скорость, с которой игрока тянет к пробоине при полном давлении.
    float suctionStrength = 9.0f;
    float suctionRadius = 7.0f;
    float maxSuctionSpeed = 14.0f;

    // Насколько быстро температура приближается к температуре снаружи.
    float coolingRate = 1.6f;
    float outsideTemperature = -80.0f;
};
