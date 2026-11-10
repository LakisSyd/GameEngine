#pragma once

#include <algorithm>
#include <cmath>

#include <ECSRegistry.h>

#include "NPCComponents.h"
#include "Components.h"

#include "Gameplay/Repair/RepairComponent.h"
#include "Gameplay/Power/GeneratorComponent.h"
#include "Gameplay/Power/OxygenGeneratorComponent.h"


// =============================================================================
// NpcUtilitySystem
// =============================================================================
//
// Utility AI отвечает только на вопрос:
//
//      "Что NPC сейчас выгоднее всего делать?"
//
// Система НЕ двигает NPC и НЕ ремонтирует объекты.
// Она только:
//      1. вычисляет score;
//      2. выбирает лучшую цель;
//      3. записывает repairTarget;
//      4. меняет NpcState.
//
// Исполнение находится в StateSystem.
//
// Сейчас есть два варианта:
//
//      Idle
//      Repair
//
// Но Repair может означать:
//
//      - пробоину корпуса;
//      - повреждённое оборудование.
//
// Для них используются разные формулы полезности,
// но repairTarget остаётся один.
//
// =============================================================================

class NpcUtilitySystem
{
public:

    void Update(
        ECSRegistry& registry,
        float deltaTime)
    {
        (void)deltaTime;

        registry.ForEach<NPCComponent, NpcBrainComponent>(
            [&](Entity npc,
                NPCComponent&,
                NpcBrainComponent& brain)
            {
                EvaluateNpc(
                    registry,
                    npc,
                    brain);
            });
    }


private:

    // =========================================================================
    // ВЫБОР ЛУЧШЕЙ ЗАДАЧИ
    // =========================================================================

    static void EvaluateNpc(
        ECSRegistry& registry,
        Entity npc,
        NpcBrainComponent& brain)
    {
        // ---------------------------------------------------------------------
        // Базовая полезность бездействия.
        // ---------------------------------------------------------------------

        brain.idleScore = 0.10f;


        // ---------------------------------------------------------------------
        // Общий победитель среди ВСЕХ ремонтных целей.
        // ---------------------------------------------------------------------

        Entity bestRepairTarget =
            INVALID_ENTITY;

        float bestRepairScore =
            0.0f;


        // =====================================================================
        // 1. ПРОБОИНЫ
        // =====================================================================

        for (auto& [breachEntity, breach] :
            registry.GetComponents<HullBreachComponent>())
        {
            (void)breach;

            const float score =
                CalculateBreachRepairScore(
                    registry,
                    npc,
                    breachEntity);


            if (score > bestRepairScore)
            {
                bestRepairScore =
                    score;

                bestRepairTarget =
                    breachEntity;
            }
        }


        // =====================================================================
        // 2. ОБОРУДОВАНИЕ
        // =====================================================================

        for (auto& [equipmentEntity, damageable] :
            registry.GetComponents<DamageableComponent>())
        {
            (void)damageable;


            // Если вдруг какая-нибудь сущность одновременно является
            // HullBreachComponent и DamageableComponent, второй раз её
            // оценивать не надо.
            if (registry.HasComponent<HullBreachComponent>(
                equipmentEntity))
            {
                continue;
            }


            const float score =
                CalculateEquipmentRepairScore(
                    registry,
                    npc,
                    equipmentEntity);


            if (score > bestRepairScore)
            {
                bestRepairScore =
                    score;

                bestRepairTarget =
                    equipmentEntity;
            }
        }


        // ---------------------------------------------------------------------
        // Сохраняем общий score для debug UI.
        // ---------------------------------------------------------------------

        brain.repairScore =
            bestRepairScore;


        // ---------------------------------------------------------------------
        // Ручная команда имеет приоритет.
        //
        // Score всё равно вычислили, чтобы видеть его в debug UI.
        // ---------------------------------------------------------------------

        if (brain.command.IsActive())
            return;


        // =====================================================================
        // ЕСЛИ NPC УЖЕ РЕМОНТИРУЕТ
        // =====================================================================

        if (brain.state == NpcState::Repairing)
        {
            if (IsRepairTargetStillValid(
                registry,
                brain.repairTarget))
            {
                return;
            }
        }


        // =====================================================================
        // НАЗНАЧЕНИЕ НОВОЙ ЦЕЛИ
        // =====================================================================

        const Entity previousTarget =
            brain.repairTarget;


        brain.repairTarget =
            bestRepairTarget;


        if (previousTarget != bestRepairTarget)
        {
            // Цель изменилась —
            // StateSystem должен перестроить маршрут.
            brain.routeDirty = true;
        }


        // =====================================================================
        // АРБИТРАЖ: Repair против Idle
        // =====================================================================

        if (bestRepairTarget != INVALID_ENTITY &&
            bestRepairScore > brain.idleScore)
        {
            brain.state =
                NpcState::MoveToRepair;

            brain.status =
                GetRepairStatus(
                    registry,
                    bestRepairTarget);
        }
        else
        {
            brain.repairTarget =
                INVALID_ENTITY;

            brain.state =
                NpcState::Idle;

            brain.status =
                "Automatic task: idle";
        }
    }


    // =========================================================================
    // ПРОВЕРКА, НУЖНО ЛИ ВСЁ ЕЩЁ РЕМОНТИРОВАТЬ ТЕКУЩУЮ ЦЕЛЬ
    // =========================================================================

    static bool IsRepairTargetStillValid(
        ECSRegistry& registry,
        Entity target)
    {
        if (target == INVALID_ENTITY)
            return false;


        const auto* repairable =
            registry.GetComponent<RepairableComponent>(
                target);


        if (!repairable ||
            repairable->repaired)
        {
            return false;
        }


        // ---------------------------------------------------------------------
        // Пробоина
        // ---------------------------------------------------------------------

        if (const auto* breach =
            registry.GetComponent<HullBreachComponent>(
                target))
        {
            return !breach->sealed;
        }


        // ---------------------------------------------------------------------
        // Оборудование
        // ---------------------------------------------------------------------

        if (const auto* damageable =
            registry.GetComponent<DamageableComponent>(
                target))
        {
            return damageable->health <
                damageable->maxHealth;
        }


        return false;
    }


    // =========================================================================
    // ТЕКСТ ДЛЯ DEBUG UI
    // =========================================================================

    static const char* GetRepairStatus(
        ECSRegistry& registry,
        Entity target)
    {
        if (registry.HasComponent<HullBreachComponent>(
            target))
        {
            return "Automatic task: repair hull breach";
        }


        if (registry.HasComponent<GeneratorComponent>(
            target))
        {
            return "Automatic task: repair fusion generator";
        }


        if (registry.HasComponent<OxygenGeneratorComponent>(
            target))
        {
            return "Automatic task: repair oxygen generator";
        }


        if (registry.HasComponent<LightComponent>(
            target))
        {
            return "Automatic task: repair lighting";
        }


        return "Automatic task: repair equipment";
    }


    // =========================================================================
    // SCORE РЕМОНТА ПРОБОИНЫ
    // =========================================================================

    static float CalculateBreachRepairScore(
        ECSRegistry& registry,
        Entity npc,
        Entity breachEntity)
    {
        const auto* breach =
            registry.GetComponent<HullBreachComponent>(
                breachEntity);

        const auto* repairable =
            registry.GetComponent<RepairableComponent>(
                breachEntity);

        const auto* npcTransform =
            registry.GetComponent<TransformComponent>(
                npc);

        const auto* brain =
            registry.GetComponent<NpcBrainComponent>(
                npc);


        if (!breach ||
            !repairable ||
            !npcTransform ||
            !brain)
        {
            return 0.0f;
        }


        if (breach->sealed ||
            repairable->repaired)
        {
            return 0.0f;
        }


        // ---------------------------------------------------------------------
        // Расстояние NPC -> пробоина
        // ---------------------------------------------------------------------

        const float dx =
            breach->worldPosition.x -
            npcTransform->position.x;

        const float dy =
            breach->worldPosition.y -
            npcTransform->position.y;

        const float dz =
            breach->worldPosition.z -
            npcTransform->position.z;


        const float distance =
            std::sqrt(
                dx * dx +
                dy * dy +
                dz * dz);


        // ---------------------------------------------------------------------
        // За пределами detectionRange NPC её не рассматривает.
        // ---------------------------------------------------------------------

        if (brain->detectionRange > 0.0f &&
            distance > brain->detectionRange)
        {
            return 0.0f;
        }


        // ---------------------------------------------------------------------
        // Чем ближе цель — тем выше score.
        //
        // NPC рядом с целью:
        //      ~1.0
        //
        // NPC на границе detectionRange:
        //      ~0.0
        // ---------------------------------------------------------------------

        const float distanceScore =
            brain->detectionRange > 0.0f
            ? 1.0f -
            std::clamp(
                distance /
                brain->detectionRange,
                0.0f,
                1.0f)
            : 1.0f;


        // ---------------------------------------------------------------------
        // Серьёзность пробоины.
        //
        // leakRate 0.15 и выше = максимальная опасность.
        // ---------------------------------------------------------------------

        const float severity =
            std::clamp(
                breach->leakRate /
                0.15f,
                0.15f,
                1.0f);


        // ---------------------------------------------------------------------
        // Опасность важнее расстояния.
        // ---------------------------------------------------------------------

        return std::clamp(
            severity * 0.75f +
            distanceScore * 0.25f,
            0.0f,
            1.0f);
    }


    // =========================================================================
    // SCORE РЕМОНТА ОБОРУДОВАНИЯ
    // =========================================================================

    static float CalculateEquipmentRepairScore(
        ECSRegistry& registry,
        Entity npc,
        Entity equipmentEntity)
    {
        const auto* damageable =
            registry.GetComponent<DamageableComponent>(
                equipmentEntity);

        const auto* repairable =
            registry.GetComponent<RepairableComponent>(
                equipmentEntity);

        const auto* equipmentTransform =
            registry.GetComponent<TransformComponent>(
                equipmentEntity);

        const auto* npcTransform =
            registry.GetComponent<TransformComponent>(
                npc);

        const auto* brain =
            registry.GetComponent<NpcBrainComponent>(
                npc);


        if (!damageable ||
            !repairable ||
            !equipmentTransform ||
            !npcTransform ||
            !brain)
        {
            return 0.0f;
        }


        // ---------------------------------------------------------------------
        // Исправное устройство не участвует в выборе.
        // ---------------------------------------------------------------------

        if (repairable->repaired ||
            damageable->health >= damageable->maxHealth)
        {
            return 0.0f;
        }


        // ---------------------------------------------------------------------
        // Расстояние NPC -> устройство
        // ---------------------------------------------------------------------

        const float dx =
            equipmentTransform->position.x -
            npcTransform->position.x;

        const float dy =
            equipmentTransform->position.y -
            npcTransform->position.y;

        const float dz =
            equipmentTransform->position.z -
            npcTransform->position.z;


        const float distance =
            std::sqrt(
                dx * dx +
                dy * dy +
                dz * dz);


        if (brain->detectionRange > 0.0f &&
            distance > brain->detectionRange)
        {
            return 0.0f;
        }


        const float distanceScore =
            brain->detectionRange > 0.0f
            ? 1.0f -
            std::clamp(
                distance /
                brain->detectionRange,
                0.0f,
                1.0f)
            : 1.0f;


        // ---------------------------------------------------------------------
        // Степень повреждения.
        //
        // Полное HP:
        //      0.0
        //
        // Ноль HP:
        //      1.0
        // ---------------------------------------------------------------------

        float damageScore =
            0.0f;


        if (damageable->maxHealth > 0.0f)
        {
            damageScore =
                1.0f -
                std::clamp(
                    damageable->health /
                    damageable->maxHealth,
                    0.0f,
                    1.0f);
        }


        // ---------------------------------------------------------------------
        // Критичность оборудования.
        //
        // Позже это лучше вынести в отдельный компонент:
        //
        // EquipmentPriorityComponent
        //
        // Но для первой версии можно определять по типу.
        // ---------------------------------------------------------------------

        float importance =
            0.40f;


        if (registry.HasComponent<GeneratorComponent>(
            equipmentEntity))
        {
            // Энергия корабля максимально важна.
            importance =
                1.0f;
        }
        else if (
            registry.HasComponent<OxygenGeneratorComponent>(
                equipmentEntity))
        {
            // Жизнеобеспечение.
            importance =
                0.90f;
        }
        else if (
            registry.HasComponent<LightComponent>(
                equipmentEntity))
        {
            // Свет важен, но обычно не критичен.
            importance =
                0.15f;
        }


        // ---------------------------------------------------------------------
        // Итоговая формула:
        //
        //      повреждение      45%
        //      важность         40%
        //      расстояние       15%
        //
        // Поэтому критическое оборудование может победить более близкое,
        // но маловажное устройство.
        // ---------------------------------------------------------------------

        return std::clamp(
            damageScore * 0.45f +
            importance * 0.40f +
            distanceScore * 0.15f,
            0.0f,
            1.0f);
    }
};