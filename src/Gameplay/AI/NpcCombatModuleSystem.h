#pragma once

#include "../../../NPCComponents.h"
#include "../../ECS/ECSRegistry.h"
#include "NpcCombatModules.h"
#include "NpcCoverLifecycle.h"
#include "NpcNavMeshSystem.h"

// Применяет только ГРАНИЦЫ между модулями. Автоматические решения остаются в
// своих системах, а здесь централизованно удаляются хвосты выключенного слоя.
// Благодаря этому checkbox действительно останавливает модуль, а не оставляет
// старые isPeeking/exposed/flankTarget влиять на движение и анимацию.
class NpcCombatModuleSystem
{
public:
    static void Update(ECSRegistry& registry, float deltaTime)
    {
        registry.ForEach<NpcCombatModulesComponent>(
            [&](Entity npc, NpcCombatModulesComponent& modules)
            {
                const std::uint32_t currentMask = modules.EnabledMask();
                const std::uint32_t allModulesMask =
                    (NpcCombatModuleBit(NpcCombatModule::SquadTactics) << 1u) - 1u;
                const std::uint32_t previousMask = modules.runtimeMaskInitialized
                    ? modules.runtimePreviousMask
                    : allModulesMask;
                const std::uint32_t disabledMask = previousMask & ~currentMask;

                auto* combat = registry.GetComponent<NPCCombatComponent>(npc);
                auto* tactical = registry.GetComponent<NpcTacticalComponent>(npc);
                auto* brain = registry.GetComponent<NpcBrainComponent>(npc);

                if (disabledMask & NpcCombatModuleBit(NpcCombatModule::Perception))
                {
                    if (combat)
                    {
                        combat->targetVisible = false;
                        combat->rangedAimTime = 0.0f;
                    }
                }

                if (disabledMask & NpcCombatModuleBit(NpcCombatModule::Search))
                    ResetSearch(npc, combat, tactical, brain);

                if ((disabledMask &
                        NpcCombatModuleBit(NpcCombatModule::PeekCycle)) &&
                    (!tactical || !tactical->debugForcedCoverAction))
                    ResetPeek(npc, tactical, brain);

                if ((disabledMask &
                        NpcCombatModuleBit(NpcCombatModule::CoverReposition)) &&
                    (!tactical || !tactical->debugForcedCoverAction))
                    ResetReposition(npc, tactical);

                if ((disabledMask &
                        NpcCombatModuleBit(NpcCombatModule::CoverSelection)) &&
                    (!tactical || !tactical->debugForcedCoverAction))
                    ResetCover(npc, combat, tactical, brain);

                if (disabledMask & NpcCombatModuleBit(NpcCombatModule::MovingFire))
                {
                    if (tactical)
                    {
                        tactical->movingAimTime = 0.0f;
                        tactical->movingBurstShotsRemaining = 0;
                        tactical->movingBurstPauseTimer = 0.0f;
                    }
                }

                if ((disabledMask &
                        NpcCombatModuleBit(NpcCombatModule::StationaryFire)) &&
                    !modules.IsOperational(NpcCombatModule::StationaryFire) &&
                    combat)
                {
                    combat->rangedAimTime = 0.0f;
                }

                if ((disabledMask &
                        NpcCombatModuleBit(NpcCombatModule::Engagement)) &&
                    !modules.IsOperational(NpcCombatModule::Engagement))
                {
                    if (brain && brain->state == NpcState::Attacking)
                    {
                        brain->state =
                            modules.IsOperational(NpcCombatModule::Pursuit) &&
                            combat &&
                            combat->hasLastKnownPosition
                            ? NpcState::Chasing
                            : NpcState::Idle;
                    }
                }

                if ((disabledMask &
                        NpcCombatModuleBit(NpcCombatModule::Pursuit)) &&
                    !modules.IsOperational(NpcCombatModule::Pursuit))
                {
                    if (brain && brain->state == NpcState::Chasing)
                    {
                        brain->state =
                            modules.IsOperational(NpcCombatModule::Engagement) &&
                            combat &&
                            combat->targetVisible
                            ? NpcState::Attacking
                            : NpcState::Idle;
                        NpcNavMeshSystem::InvalidatePath(npc);
                    }
                }

                // Реакция на входящий огонь — автоматический сенсорный слой.
                // Когда он выключен, новые DamageEvent не должны снова
                // накапливать скрытое решение для более позднего кадра.
                if (!modules.incomingFireReaction && tactical)
                {
                    tactical->underFireTime = 0.0f;
                    tactical->damageReactionPending = false;
                    tactical->damageReactionWasHit = false;
                    tactical->recentDamageHits = 0;
                    tactical->recentDamageWindow = 0.0f;
                    tactical->hasIncomingFireDirection = false;
                    tactical->incomingFireTurnTimer = 0.0f;
                }

                if (!modules.automaticTactics && tactical)
                {
                    // Автоматический выбор выключен, но вручную закреплённый
                    // стиль остаётся авторской настройкой. Auto получает
                    // нейтральный Balanced и не пересчитывается по ситуации.
                    tactical->activeStyle =
                        tactical->selectedStyle == NpcTacticalStyle::Auto
                            ? NpcTacticalStyle::Balanced
                            : tactical->selectedStyle;
                    tactical->tacticCommitTimer = 0.0f;
                    tactical->enemyClosingSpeed = 0.0f;
                    tactical->hasThreatDistanceSample = false;
                    tactical->tacticReason =
                        "Automatic tactics module is disabled";
                }

                if (!modules.squadTactics && tactical)
                    tactical->role = NpcSquadRole::None;

                modules.TickDebugOverride(deltaTime);

                modules.runtimePreviousMask = currentMask;
                modules.runtimeMaskInitialized = true;
            });
    }

private:
    static void ResetSearch(
        Entity npc,
        NPCCombatComponent* combat,
        NpcTacticalComponent* tactical,
        NpcBrainComponent* brain)
    {
        const bool hadSearchRoute =
            (brain && brain->state == NpcState::Searching) ||
            (tactical && tactical->searchingSector);
        if (combat)
        {
            combat->searchPlanReady = false;
            combat->investigatingPrediction = false;
            combat->searchTravelTime = 0.0f;
            combat->searchTurnRemaining = 0.0f;
        }
        if (tactical)
        {
            if (tactical->searchingSector)
            {
                tactical->hasFlankTarget = false;
                tactical->flankCommitTime = 0.0f;
            }
            tactical->searchingSector = false;
            tactical->searchSectorsAssigned = 0;
            tactical->searchRouteFailures = 0;
            tactical->searchRetryTime = 0.0f;
        }
        // Не переводим Searching в Idle здесь: StateSystem в том же кадре
        // увидит выключенный gate, вызовет ForgetTarget() и очистит личную
        // память. Ранний Idle оставил бы вечную lastKnownPosition.
        if (hadSearchRoute)
            NpcNavMeshSystem::InvalidatePath(npc);
    }

    static void ResetPeek(
        Entity npc,
        NpcTacticalComponent* tactical,
        NpcBrainComponent* brain)
    {
        if (!tactical)
            return;

        const bool mustReturnBehindCover =
            (tactical->movingToPeek || tactical->exposed) &&
            (tactical->reservedCover != INVALID_ENTITY ||
                tactical->dynamicCoverActive);

        tactical->movingToPeek = false;
        tactical->exposed = false;
        tactical->isPeeking = false;
        tactical->wantsPeek = false;
        tactical->peekState = NpcTacticalComponent::PeekSide::none;
        tactical->phaseTimer = 0.0f;
        tactical->crouched = tactical->coverUsesCrouch;

        if (mustReturnBehindCover && brain)
        {
            tactical->moveTarget = tactical->coverPosition;
            brain->state = NpcState::TakingCover;
            NpcNavMeshSystem::InvalidatePath(npc);
        }
    }

    static void ResetReposition(
        Entity npc,
        NpcTacticalComponent* tactical)
    {
        if (!tactical)
            return;
        // searchingSector тоже использует hasFlankTarget как временную
        // NavMesh-точку. Выключение боевой смены позиции не имеет права
        // обрывать независимый модуль поиска.
        const bool hadRoute = !tactical->searchingSector &&
            (tactical->hasFlankTarget ||
                tactical->coverChangeRequested);
        if (!tactical->searchingSector)
            tactical->hasFlankTarget = false;
        tactical->coverChangeRequested = false;
        if (!tactical->searchingSector)
            tactical->flankCommitTime = 0.0f;
        if (hadRoute)
            NpcNavMeshSystem::InvalidatePath(npc);
    }

    static void ResetCover(
        Entity npc,
        const NPCCombatComponent* combat,
        NpcTacticalComponent* tactical,
        NpcBrainComponent* brain)
    {
        if (!tactical)
            return;

        NpcCoverLifecycle::Clear(*tactical);

        if (brain && (brain->state == NpcState::TakingCover ||
            brain->state == NpcState::InCover))
        {
            brain->state = combat && combat->hasLastKnownPosition
                ? NpcState::Chasing
                : NpcState::Idle;
        }
        NpcNavMeshSystem::InvalidatePath(npc);
    }
};
