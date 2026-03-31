#include "PressureSystem.h"
#include "Components.h"
#include <BulletDynamics/Dynamics/btRigidBody.h>

void PressureSystem::Update(ECSRegistry& registry, float deltaTime, float waterLevel)
{
    constexpr float ATMOSPHERIC_PRESSURE = 101325.0f; // Па (1 атм)
    constexpr float WATER_DENSITY = 1000.0f;          // кг/м³
    constexpr float GRAVITY = 9.81f;                  // м/с²

    auto& hullPool = registry.GetComponents<PressureHullComponent>();
    auto& rigidPool = registry.GetComponents<RigidBodyComponent>();

    for (auto& [entity, hull] : hullPool)
    {
        // 1. Проверяем, есть ли у объекта физическое тело
        if (!registry.HasComponent<RigidBodyComponent>(entity))
            continue;

        auto* rb = rigidPool.Get(entity);
        if (!rb || !rb->body)
            continue;

        // 2. Получаем позицию и считаем глубину
        btVector3 pos = rb->body->getWorldTransform().getOrigin();
        float depth = waterLevel - pos.getY();

        // Если объект над водой — глубина 0
        if (depth < 0.0f)
            depth = 0.0f;

        // Сохраняем для отладки
        hull.lastDepth = depth;

        // 3. Считаем давление: P = P₀ + ρ·g·h
        float pressure = ATMOSPHERIC_PRESSURE + (WATER_DENSITY * GRAVITY * depth);
        hull.lastPressure = pressure;

        // 4. Если давление превышает лимит — наносим урон
        if (pressure > hull.maxPressure)
        {
            float pressureRatio = pressure / hull.maxPressure; // 1.0 = на пределе
            float damagePerSecond = (pressureRatio - 1.0f) * 20.0f; // Настрой под геймплей

            hull.currentHealth -= damagePerSecond * deltaTime;
            hull.currentHealth = fmaxf(0.0f, hull.currentHealth);

            // Включаем визуальные эффекты утечки
            hull.isLeaking = (hull.currentHealth < 100.0f);
            hull.leakIntensity = fminf(1.0f, (100.0f - hull.currentHealth) / 50.0f);
        }
        else
        {
            // Медленный саморемонт, если нет повреждений
            if (hull.currentHealth < 100.0f)
            {
                hull.currentHealth = fminf(100.0f, hull.currentHealth + hull.repairRate * deltaTime * 0.1f);
            }

            if (hull.currentHealth >= 100.0f)
            {
                hull.isLeaking = false;
                hull.leakIntensity = 0.0f;
            }
        }

        // ── ОТЛАДКА: вывод в консоль ────────────────────────────────────
        
        
        
    }
}