#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>
#include <windows.h>

#include <ECSRegistry.h>
#include <InputClass.h>
#include <Physics/PhysicsRaycast.h>
#include <MeshCuttingTest.h>

#include "Gameplay/Power/GeneratorComponent.h"
#include "Gameplay/Power/OxygenGeneratorComponent.h"
#include "Gameplay/Repair/RuntimeMeshDamageComponent.h"

#include "../../WorldEventLog.h"
#include "RepairComponent.h"


namespace{
    Entity FindRoomForEntity(
        ECSRegistry& registry,
        Entity entity)
    {
        const auto* membership =
            registry.GetComponent<RoomMembershipComponent>(
                entity);

        if (!membership)
            return INVALID_ENTITY;

        return membership->room;
    }
}

class RepairSystem
{
public:

    RepairSystem() = default;
    ~RepairSystem() = default;


    void Update(
        ECSRegistry& registry,
        float deltaTime,
        InputClass* input,
        CameraClass* camera,
        btDynamicsWorld* world,
        JobSystem& jobs,
        WorldEventLog& eventLog,
        double worldTime)
    {
        if (deltaTime <= 0.0f)
            return;


        // ---------------------------------------------------------
        // Проверяем завершившиеся асинхронные ремонты.
        //
        // Это должно происходить независимо от того,
        // нажимает ли игрок сейчас кнопку ремонта.
        // ---------------------------------------------------------

        PumpCompletedRepairs(
            registry,
            jobs,
            eventLog,
            worldTime);


        if (!input || !camera || !world)
            return;


        // ---------------------------------------------------------
        // Ищем игрока
        // ---------------------------------------------------------

        Entity player = INVALID_ENTITY;

        registry.ForEach<PlayerGameTag>(
            [&](Entity entity, PlayerGameTag&)
            {
                if (player == INVALID_ENTITY)
                    player = entity;
            });


        if (player == INVALID_ENTITY)
            return;


        // ---------------------------------------------------------
        // Получаем CharacterController игрока
        // ---------------------------------------------------------

        auto* controller =
            registry.GetComponent<CharacterController>(player);

        if (!controller)
            return;


        // ---------------------------------------------------------
        // Ремонт выполняется при удержании L
        // ---------------------------------------------------------

        if (!input->IsKeyDown('L'))
            return;


        // ---------------------------------------------------------
        // Raycast из камеры
        // ---------------------------------------------------------

        PhysicsRaycastHit rayHit;

        PhysicsRaycast::CastFromCamera(
            camera,
            world,
            12.0f,
            player,
            controller->ghostObj,
            rayHit);


        // ---------------------------------------------------------
        // Ищем подходящую цель ремонта
        // ---------------------------------------------------------

        const Entity repairEntity =
            FindRepairTarget(
                registry,
                rayHit);


        if (repairEntity == INVALID_ENTITY)
            return;


        auto* repairable =
            registry.GetComponent<RepairableComponent>(
                repairEntity);


        if (!repairable || repairable->repaired)
            return;


        // ---------------------------------------------------------
        // Применяем прогресс ремонта.
        //
        // Событие здесь НЕ создаём.
        //
        // Оно создаётся внутри CompleteRepair(),
        // потому что только там мы точно знаем,
        // что пробоина действительно закрыта.
        // ---------------------------------------------------------

        ApplyRepairProgress(
            registry,
            repairEntity,
            repairable->repairRate * deltaTime,
            &jobs,
            &eventLog,
            worldTime,
            player);
    }


    // =============================================================
    //
    // ЕДИНАЯ ТОЧКА РЕМОНТА
    //
    // Используется игроком, NPC и будущими инструментами.
    //
    // progressDelta измеряется в единицах
    // RepairableComponent::progress.
    //
    // Возвращает true только когда объект действительно
    // полностью восстановлен.
    //
    // =============================================================

    static bool ApplyRepairProgress(
        ECSRegistry& registry,
        Entity repairEntity,
        float progressDelta,
        JobSystem* jobs = nullptr,
        WorldEventLog* eventLog = nullptr,
        double worldTime = 0.0,
        Entity repairSource = INVALID_ENTITY)
    {
        auto* repairable =
            registry.GetComponent<RepairableComponent>(
                repairEntity);


        if (!repairable ||
            repairable->repaired ||
            progressDelta <= 0.0f)
        {
            return false;
        }


        repairable->progress += progressDelta;

        repairable->progress =
            (std::min)(
                repairable->progress,
                repairable->requiredProgress);


        if (repairable->progress <
            repairable->requiredProgress)
        {
            return false;
        }


        return CompleteRepair(
            registry,
            repairEntity,
            *repairable,
            jobs,
            eventLog,
            worldTime,
            repairSource);
    }


private:

    // =============================================================
    //
    // ПОИСК ЦЕЛИ РЕМОНТА
    //
    // =============================================================

    static Entity FindRepairTarget(
        ECSRegistry& registry,
        const PhysicsRaycastHit& rayHit)
    {
        Entity bestRayEntity =
            FindRepairTargetAlongRay(
                registry,
                rayHit);


        if (bestRayEntity != INVALID_ENTITY)
            return bestRayEntity;


        // ---------------------------------------------------------
        // Прямое попадание в пробоину или устройство
        // ---------------------------------------------------------

        if (rayHit.entity != INVALID_ENTITY)
        {
            if (auto* breach =
                registry.GetComponent<HullBreachComponent>(
                    rayHit.entity))
            {
                if (!breach->sealed &&
                    registry.HasComponent<RepairableComponent>(
                        rayHit.entity))
                {
                    return rayHit.entity;
                }
            }


            // -----------------------------------------------------
            // Устройства ремонтируются только если получили урон.
            // -----------------------------------------------------

            if (auto* damageable =
                registry.GetComponent<DamageableComponent>(
                    rayHit.entity))
            {
                const auto* repairable =
                    registry.GetComponent<RepairableComponent>(
                        rayHit.entity);


                if (repairable &&
                    !repairable->repaired &&
                    damageable->health < damageable->maxHealth)
                {
                    return rayHit.entity;
                }
            }
        }


        // ---------------------------------------------------------
        // Если луч попал в панель корпуса,
        // ищем связанную с ней ближайшую пробоину.
        // ---------------------------------------------------------

        if (!registry.HasComponent<HullPanelComponent>(
            rayHit.entity))
        {
            return INVALID_ENTITY;
        }


        Entity bestEntity = INVALID_ENTITY;

        float bestDistanceSq =
            (std::numeric_limits<float>::max)();


        for (auto& [breachEntity, breach] :
            registry.GetComponents<HullBreachComponent>())
        {
            if (breach.panel != rayHit.entity ||
                breach.sealed)
            {
                continue;
            }


            if (!registry.HasComponent<RepairableComponent>(
                breachEntity))
            {
                continue;
            }


            const float dx =
                breach.worldPosition.x -
                rayHit.position.x;

            const float dy =
                breach.worldPosition.y -
                rayHit.position.y;

            const float dz =
                breach.worldPosition.z -
                rayHit.position.z;


            const float distanceSq =
                dx * dx +
                dy * dy +
                dz * dz;


            const float allowedDistance =
                (std::max)(
                    breach.radius * 2.5f,
                    0.75f);


            if (distanceSq <=
                allowedDistance * allowedDistance &&
                distanceSq < bestDistanceSq)
            {
                bestDistanceSq =
                    distanceSq;

                bestEntity =
                    breachEntity;
            }
        }


        return bestEntity;
    }


    // =============================================================
    //
    // ПОИСК ПРОБОИНЫ ВДОЛЬ ЛУЧА
    //
    // =============================================================

    static Entity FindRepairTargetAlongRay(
        ECSRegistry& registry,
        const PhysicsRaycastHit& rayHit)
    {
        const btVector3 rayStart =
            rayHit.rayStart;

        const btVector3 rayEnd =
            rayHit.rayEnd;

        const btVector3 rayDelta =
            rayEnd - rayStart;


        const float rayLengthSq =
            rayDelta.length2();


        if (rayLengthSq <= 0.0001f)
            return INVALID_ENTITY;


        Entity bestEntity =
            INVALID_ENTITY;

        float bestForwardDistance =
            (std::numeric_limits<float>::max)();


        for (auto& [breachEntity, breach] :
            registry.GetComponents<HullBreachComponent>())
        {
            if (breach.sealed)
                continue;


            auto* repairable =
                registry.GetComponent<RepairableComponent>(
                    breachEntity);


            if (!repairable ||
                repairable->repaired)
            {
                continue;
            }


            const btVector3 breachPos(
                breach.worldPosition.x,
                breach.worldPosition.y,
                breach.worldPosition.z);


            const btVector3 toBreach =
                breachPos - rayStart;


            float t =
                toBreach.dot(rayDelta) /
                rayLengthSq;


            t =
                (std::max)(
                    0.0f,
                    (std::min)(
                        t,
                        1.0f));


            const btVector3 closest =
                rayStart +
                rayDelta * t;


            const float distanceSq =
                (breachPos - closest)
                .length2();


            const float allowedDistance =
                (std::max)(
                    breach.radius * 2.75f,
                    0.85f);


            if (distanceSq >
                allowedDistance * allowedDistance)
            {
                continue;
            }


            const float forwardDistance =
                (closest - rayStart)
                .length();


            if (forwardDistance <
                bestForwardDistance)
            {
                bestForwardDistance =
                    forwardDistance;

                bestEntity =
                    breachEntity;
            }
        }


        return bestEntity;
    }


    // =============================================================
    //
    // ФАКТИЧЕСКОЕ ЗАВЕРШЕНИЕ РЕМОНТА
    //
    // Именно здесь создаём WorldEvent,
    // потому что только здесь состояние мира реально меняется.
    //
    // =============================================================

    static bool CompleteRepair(
        ECSRegistry& registry,
        Entity repairEntity,
        RepairableComponent& repairable,
        JobSystem* jobs,
        WorldEventLog* eventLog,
        double worldTime,
        Entity repairSource)
    {
        // ---------------------------------------------------------
        // ПРОБОИНА КОРПУСА
        // ---------------------------------------------------------

        auto* breach =
            registry.GetComponent<HullBreachComponent>(
                repairEntity);


        if (breach && !breach->sealed)
        {
            // -----------------------------------------------------
            // Сначала восстанавливаем runtime mesh и collision.
            //
            // Если async задача ещё не завершена —
            // возвращаем false.
            //
            // Прогресс при этом остаётся 100%.
            // -----------------------------------------------------

            if (const auto* repairLink =
                registry.GetComponent<
                HullBreachRepairLinkComponent>(
                    repairEntity))
            {
                const bool meshReady =
                    jobs
                    ? MeshCuttingTest::RepairCutAsync(
                        registry,
                        *jobs,
                        repairLink->cutOperationId)

                    : MeshCuttingTest::RepairCut(
                        registry,
                        repairLink->cutOperationId);


                if (!meshReady)
                    return false;
            }


            // -----------------------------------------------------
            // Запоминаем данные события ДО DestroyEntity.
            // -----------------------------------------------------

            const Entity roomEntity =
                breach->room;

            const float breachRadius =
                breach->radius;


            // -----------------------------------------------------
            // Теперь пробоина действительно закрыта.
            // -----------------------------------------------------

            breach->sealed = true;
            breach->leakRate = 0.0f;


            // -----------------------------------------------------
            // Обновляем состояние панели корпуса
            // -----------------------------------------------------

            if (auto* panel =
                registry.GetComponent<HullPanelComponent>(
                    breach->panel))
            {
                panel->breachCount =
                    CountOpenBreaches(
                        registry,
                        breach->panel);


                panel->isDestroyed =
                    panel->breachCount >=
                    panel->maxBreaches;


                if (panel->breachCount == 0)
                {
                    panel->health =
                        panel->maxHealth;

                    panel->accumulatedDamage =
                        0.0f;
                }
                else
                {
                    panel->health =
                        (std::max)(
                            panel->health,
                            panel->maxHealth * 0.35f);
                }


                RemoveRepairedDamageDecal(
                    registry,
                    breach->panel,
                    *breach);
            }


            repairable.repaired = true;


            // -----------------------------------------------------
            // Записываем мировой факт:
            // пробоина действительно была закрыта.
            // -----------------------------------------------------

            if (eventLog)
            {
                WorldEvent event;

                event.type =
                    WorldEventType::BreachSealed;

                event.timeSeconds =
                    worldTime;

                event.source =
                    repairSource;

                event.subject =
                    repairEntity;

                event.room =
                    roomEntity;

                event.severity =
                    breachRadius;


                eventLog->Push(event);
            }


            OutputDebugStringA(
                "[Repair] Hull breach sealed.\n");


            // -----------------------------------------------------
            // После создания события сущность пробоины
            // больше не нужна.
            // -----------------------------------------------------

            registry.DestroyEntity(
                repairEntity);


            return true;
        }


        // ---------------------------------------------------------
        // ОБЫЧНОЕ ПОВРЕЖДЁННОЕ УСТРОЙСТВО
        // ---------------------------------------------------------

        if (auto* damageable =
            registry.GetComponent<DamageableComponent>(
                repairEntity))
        {
            damageable->health =
                damageable->maxHealth;

            damageable->isDestroyed =
                false;


            if (auto* generator =
                registry.GetComponent<GeneratorComponent>(
                    repairEntity))
            {
                generator->isOnline =
                    true;
            }


            if (auto* oxygenGenerator =
                registry.GetComponent<OxygenGeneratorComponent>(
                    repairEntity))
            {
                oxygenGenerator->broken =
                    false;
            }


            repairable.repaired =
                true;

            if (eventLog)
            {
                WorldEvent event;

                event.type =
                    WorldEventType::EquipmentRepaired;

                event.timeSeconds =
                    worldTime;

                event.source =
                    repairSource;

                event.subject =
                    repairEntity;

                event.room = FindRoomForEntity(registry, repairEntity);
                    
                if (auto* name =
                    registry.GetComponent<NameComponent>(repairEntity))
                {
                    OutputDebugStringA(name->name.c_str());
                    OutputDebugStringA("\n");
                }
                


                eventLog->Push(event);
            }


            OutputDebugStringA(
                "[Repair] Device restored.\n");


            return true;
        }


        return false;
    }


    // =============================================================
    //
    // ЗАВЕРШЕНИЕ ASYNC РЕМОНТОВ
    //
    // =============================================================

    static void PumpCompletedRepairs(
        ECSRegistry& registry,
        JobSystem& jobs,
        WorldEventLog& eventLog,
        double worldTime)
    {
        // ---------------------------------------------------------
        // CompleteRepair может уничтожить breach entity,
        // поэтому нельзя удалять сущности во время прямого
        // обхода component pool.
        // ---------------------------------------------------------

        std::vector<Entity> readyEntities;


        for (auto& [entity, repairable] :
            registry.GetComponents<RepairableComponent>())
        {
            if (!repairable.repaired &&
                repairable.progress >=
                repairable.requiredProgress)
            {
                readyEntities.push_back(
                    entity);
            }
        }


        for (Entity entity :
        readyEntities)
        {
            auto* repairable =
                registry.GetComponent<RepairableComponent>(
                    entity);


            if (!repairable ||
                repairable->repaired)
            {
                continue;
            }


            // -----------------------------------------------------
            // В случае async завершения мы можем уже не знать,
            // кто именно начал ремонт.
            //
            // Поэтому source = INVALID_ENTITY.
            //
            // Позже это можно хранить в RepairableComponent
            // или отдельной runtime repair task.
            // -----------------------------------------------------

            CompleteRepair(
                registry,
                entity,
                *repairable,
                &jobs,
                &eventLog,
                worldTime,
                INVALID_ENTITY);
        }
    }


    // =============================================================
    //
    // ПОДСЧЁТ ОТКРЫТЫХ ПРОБОИН
    //
    // =============================================================

    static int CountOpenBreaches(
        ECSRegistry& registry,
        Entity panelEntity)
    {
        int count = 0;


        for (auto& [breachEntity, breach] :
            registry.GetComponents<HullBreachComponent>())
        {
            (void)breachEntity;


            if (breach.panel == panelEntity &&
                !breach.sealed)
            {
                ++count;
            }
        }


        return count;
    }


    // =============================================================
    //
    // УДАЛЕНИЕ DAMAGE DECAL ПОСЛЕ РЕМОНТА
    //
    // =============================================================

    static void RemoveRepairedDamageDecal(
        ECSRegistry& registry,
        Entity panelEntity,
        const HullBreachComponent& breach)
    {
        auto* decals =
            registry.GetComponent<DamageDecalComponent>(
                panelEntity);


        if (!decals ||
            decals->cracks.empty())
        {
            return;
        }


        auto nearest =
            decals->cracks.end();


        float nearestDistanceSq =
            (std::numeric_limits<float>::max)();


        for (auto it =
            decals->cracks.begin();
            it != decals->cracks.end();
            ++it)
        {
            const float dx =
                it->localPosition.x -
                breach.localPosition.x;

            const float dy =
                it->localPosition.y -
                breach.localPosition.y;

            const float dz =
                it->localPosition.z -
                breach.localPosition.z;


            const float distanceSq =
                dx * dx +
                dy * dy +
                dz * dz;


            if (distanceSq <
                nearestDistanceSq)
            {
                nearestDistanceSq =
                    distanceSq;

                nearest =
                    it;
            }
        }


        const float allowedDistance =
            (std::max)(
                breach.radius * 2.0f,
                0.5f);


        if (nearest !=
            decals->cracks.end() &&
            nearestDistanceSq <=
            allowedDistance *
            allowedDistance)
        {
            decals->cracks.erase(
                nearest);
        }
    }
};
