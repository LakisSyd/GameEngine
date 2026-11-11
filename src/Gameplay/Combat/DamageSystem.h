#pragma once
#include <deque>
#include "DamageEvent.h"
#include <ECSRegistry.h>
#include "../../../WorldEventLog.h"

class ParticleSystem;

class DamageSystem
{
public:
    DamageSystem() = default;
    ~DamageSystem() = default;

    void AddDamageEvent(const DamageEvent& damageEvent)
    {
        damageQueue.push_back(damageEvent);
    }

    void Update(
        ECSRegistry& registry,
        ParticleSystem& particleSystem,
        WorldEventLog& eventLog,
        double worldTime,
        float deltaTime);

private:
    std::deque<DamageEvent> damageQueue;
};
