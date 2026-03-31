#pragma once
#pragma once
#include "ECSRegistry .h"

class FloodingSystem
{
public:
    // ── Обновление затопления всех отсеков ────────────────────────────────────
    static void Update(ECSRegistry& registry, float deltaTime, float globalWaterLevel);

    // ── Создание пробоины ──────────────────────────────────────────────────────
    static void CreateBreach(ECSRegistry& registry, Entity compartment,
        const XMFLOAT3& localPosition, float size = 0.5f);

    void CreateBreachAtWorldPosition(ECSRegistry& registry, Entity compartment, const XMFLOAT3& worldPosition, float size);

    // ── Ремонт пробоины ────────────────────────────────────────────────────────
    static void SealBreach(ECSRegistry& registry, Entity compartment, size_t breachIndex);

    // ── Проверка, в каком отсеке находится объект ──────────────────────────────
    static Entity FindCompartmentForObject(ECSRegistry& registry, const XMFLOAT3& worldPos);

private:
    // Рассчитать скорость затопления через пробоину
    static float CalculateFlowRate(const BreachData& breach, float waterPressure);
};
