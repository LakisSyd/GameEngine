#pragma once
#include "ECSRegistry .h"
#include "Components.h"
#include "PhysicsSystem.h"
#include <DirectXMath.h>

using namespace DirectX;

class SubmarineSystem
{
public:
    // 🔧 Главный метод: безопасно добавить отсек
    static Entity AddCompartmentToSubmarine(
        ECSRegistry& registry,
        Entity submarineRoot,
        const XMFLOAT3& localOffset,
        const XMFLOAT3& compartmentSize,
        float compartmentMass)
    {
        // ✅ Проверка: корень существует?
        auto* rootTransform = registry.GetComponent<TransformComponent>(submarineRoot);
        auto* rootSub = registry.GetComponent<SubmarineComponent>(submarineRoot);

        if (!rootTransform || !rootSub)
        {
            OutputDebugStringA("❌ ERROR: Invalid submarine root\n");
            return INVALID_ENTITY;
        }

        // ✅ Создаём сущность отсека
        Entity compartment = registry.CreateEntity();

        // ✅ Transform (позиция = позиция подлодки + локальный offset)
        XMFLOAT3 worldPos = rootTransform->position;
        registry.AddComponent<TransformComponent>(compartment, TransformComponent{
            .position = worldPos,
            .rotation = {0, 0, 0},
            .scale = {1, 1, 1}
            });

        // ✅ CompartmentComponent с заданными размерами
        CompartmentComponent comp{};
        comp.localMin = {
            -compartmentSize.x * 0.5f,
            -compartmentSize.y * 0.5f,
            -compartmentSize.z * 0.5f
        };
        comp.localMax = {
            compartmentSize.x * 0.5f,
            compartmentSize.y * 0.5f,
            compartmentSize.z * 0.5f
        };
        comp.localCenter = localOffset;
        comp.RecalculateVolume();
        comp.CacheSize();
        registry.AddComponent<CompartmentComponent>(compartment, comp);

        // ✅ SubmarineComponent (ссылка на корень)
        registry.AddComponent<SubmarineComponent>(compartment, SubmarineComponent{
            .submarineRoot = submarineRoot
            });

        // ✅ MeshRendererComponent для визуализации
        // (опционально - можно вынести в SceneManager)

        // ✅ КРИТИЧЕСКИ ВАЖНО: Добавляем в список
        rootSub->allCompartments.push_back(compartment);

        // ✅ КРИТИЧЕСКИ ВАЖНО: Отмечаем, что нужно обновить физику
        rootSub->needsPhysicsUpdate = true;

        OutputDebugStringA(
            ("✅ Added compartment #" + std::to_string(compartment) +
                " to submarine #" + std::to_string(submarineRoot) +
                " (volume: " + std::to_string(comp.volume) + ")\n").c_str()
        );

        return compartment;
    }

    // 🔧 Пересчитать массу и буойантность подлодки на основе всех отсеков
    static void RecalculateSubmarineMass(ECSRegistry& registry, Entity submarineRoot)
    {
        auto* rootRb = registry.GetComponent<RigidBodyComponent>(submarineRoot);
        auto* rootBuoy = registry.GetComponent<BuoyancyComponent>(submarineRoot);
        auto* rootSub = registry.GetComponent<SubmarineComponent>(submarineRoot);

        if (!rootRb || !rootBuoy || !rootSub)
        {
            OutputDebugStringA("❌ ERROR: Missing components for submarine\n");
            return;
        }

        // 🔥 Базовая масса подлодки (корпус)
        float totalMass = 3000.0f;  // Масса корпуса
        float totalVolume = 0.0f;

        // 🔥 Добавляем массу каждого отсека
        for (Entity compartmentEntity : rootSub->allCompartments)
        {
            auto* comp = registry.GetComponent<CompartmentComponent>(compartmentEntity);
            if (comp)
            {
                totalMass += 500.0f;  // Масса материала отсека
                totalVolume += comp->volume;
            }
        }

        // 🔥 Применяем новые значения
        rootRb->mass = totalMass;
        rootBuoy->volume = totalVolume + 15.0f;  // +15 м³ за корпус
        rootBuoy->lastCalculatedVolume = rootBuoy->volume;

        // 🔥 Кэшируем в SubmarineComponent
        rootSub->cachedTotalMass = totalMass;
        rootSub->cachedTotalVolume = rootBuoy->volume;

        OutputDebugStringA(
            ("✅ Updated submarine physics:\n"
                "   Total mass: " + std::to_string((int)totalMass) + " kg\n" +
                "   Total volume: " + std::to_string(totalVolume) + " m³\n").c_str()
        );
    }

    // 🔧 Пересоздать физическое тело подлодки
    static void RefreshSubmarinePhysics(ECSRegistry& registry, Entity submarineRoot)
    {
        auto* rootRb = registry.GetComponent<RigidBodyComponent>(submarineRoot);

        if (!rootRb)
        {
            OutputDebugStringA("❌ ERROR: No RigidBody on submarine\n");
            return;
        }

        // 🔥 Удаляем старое тело (сохраняем velocity!)
        btVector3 savedLinearVelocity(0, 0, 0);
        btVector3 savedAngularVelocity(0, 0, 0);

        if (rootRb->body)
        {
            savedLinearVelocity = rootRb->body->getLinearVelocity();
            savedAngularVelocity = rootRb->body->getAngularVelocity();

            PhysicsWorld::Get().GetWorld()->removeRigidBody(rootRb->body);
            delete rootRb->body->getMotionState();
            delete rootRb->body;
            rootRb->body = nullptr;
        }

        // 🔥 Пересчитываем физику
        RecalculateSubmarineMass(registry, submarineRoot);

        // 🔥 Создаём новое тело
        PhysicsSystem::CreateRigidBody(registry, submarineRoot);

        // 🔥 Восстанавливаем velocity (подлодка не должна замерзнуть)
        if (rootRb->body)
        {
            rootRb->body->setLinearVelocity(savedLinearVelocity);
            rootRb->body->setAngularVelocity(savedAngularVelocity);
        }

        OutputDebugStringA("✅ Refreshed submarine physics\n");
    }

    // 🔧 Проверить, нужно ли обновить физику
    static bool NeedsPhysicsUpdate(ECSRegistry& registry, Entity submarineRoot)
    {
        auto* rootSub = registry.GetComponent<SubmarineComponent>(submarineRoot);
        if (!rootSub) return false;

        // ✅ Проверяем флаг
        if (rootSub->needsPhysicsUpdate) return true;

        // ✅ Проверяем, изменилось ли количество отсеков
        if (rootSub->compartmentCountLastFrame != (int)rootSub->allCompartments.size())
            return true;

        // ✅ Проверяем, изменился ли размер отсеков
        for (Entity compartmentEntity : rootSub->allCompartments)
        {
            auto* comp = registry.GetComponent<CompartmentComponent>(compartmentEntity);
            if (comp && comp->HasSizeChanged())
                return true;
        }

        return false;
    }

    // 🔧 Обновить подлодку (вызывать каждый кадр в Update)
    static void UpdateSubmarine(ECSRegistry& registry, Entity submarineRoot)
    {
        if (!NeedsPhysicsUpdate(registry, submarineRoot))
            return;

        auto* rootSub = registry.GetComponent<SubmarineComponent>(submarineRoot);
        if (!rootSub) return;

        // 🔥 Пересчитываем и обновляем физику
        RefreshSubmarinePhysics(registry, submarineRoot);

        // 🔥 Обновляем состояние
        rootSub->needsPhysicsUpdate = false;
        rootSub->compartmentCountLastFrame = (int)rootSub->allCompartments.size();

        // 🔥 Кэшируем размеры всех отсеков
        for (Entity compartmentEntity : rootSub->allCompartments)
        {
            auto* comp = registry.GetComponent<CompartmentComponent>(compartmentEntity);
            if (comp) comp->CacheSize();
        }
    }
};
