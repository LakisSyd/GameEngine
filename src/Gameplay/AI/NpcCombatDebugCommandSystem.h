#pragma once

#include <algorithm>
#include <limits>
#include <string>
#include <unordered_map>

#include "../../../NpcCoverQuery.h"
#include "../../../NPCComponents.h"
#include "../../ECS/ECSRegistry.h"
#include "NpcCombatMath.h"
#include "NpcCoverLifecycle.h"
#include "NpcNavMeshSystem.h"
#include "NpcSimulationStudio.h"

// Исполнитель одноразовых команд Combat Lab. UI только ставит запрос в очередь;
// все предусловия и изменения ECS сосредоточены здесь и выполняются игровым
// тиком. Команды не создают магические попадания и не обходят WeaponSystem.
class NpcCombatDebugCommandSystem
{
public:
    static void ProcessPending(
        ECSRegistry& registry,
        btDynamicsWorld* world,
        double timeSeconds)
    {
        NpcDebugCombatActionRequest request;
        if (!NpcSimulationStudio::ConsumeCombatAction(request))
            return;

        std::string result;
        const bool succeeded = Execute(
            registry,
            world,
            request,
            result);
        NpcSimulationStudio::ReportCombatAction(succeeded, result);
        NpcSimulationStudio::PushEvent(
            timeSeconds,
            request.npc,
            succeeded ? NpcDebugEventCategory::Debug
                      : NpcDebugEventCategory::Warning,
            succeeded ? "Команда Combat Lab выполнена"
                      : "Команда Combat Lab отклонена",
            result,
            succeeded ? "Combat Lab command executed"
                      : "Combat Lab command rejected",
            result);
    }

    // Вызывается после обычных StateSystem и CharacterFacingSystem. Поэтому
    // короткий принудительный уклон имеет последний голос только в движении
    // этого кадра, но всё равно проходит настоящий CharacterController.
    static void ApplyMovementOverrides(ECSRegistry& registry, float deltaTime)
    {
        for (auto it = s_dodges.begin(); it != s_dodges.end();)
        {
            const Entity npc = it->first;
            ActiveDodge& dodge = it->second;
            auto* movement = registry.GetComponent<CharacterMovement>(npc);
            auto* transform = registry.GetComponent<TransformComponent>(npc);
            auto* combat = registry.GetComponent<NPCCombatComponent>(npc);

            if (!movement || !transform || !combat ||
                !registry.IsAlive(combat->targetEntity))
            {
                it = s_dodges.erase(it);
                continue;
            }

            const auto* target = registry.GetComponent<TransformComponent>(
                combat->targetEntity);
            if (!target)
            {
                it = s_dodges.erase(it);
                continue;
            }

            DirectX::XMFLOAT3 direction = NpcCombatMath::HorizontalLeft(
                transform->position,
                target->position);
            if (!dodge.left)
            {
                direction.x = -direction.x;
                direction.z = -direction.z;
            }

            movement->walkDirection = direction;
            movement->isMoving = true;
            movement->wantsRun = true;
            movement->isRunning = true;
            movement->isCrouching = false;
            movement->currentSpeed = (std::max)(
                movement->runSpeed,
                movement->moveSpeed);
            movement->locomotionState = PlayerLocomotionState::Run;

            // Уклон является боковым перемещением: корпус продолжает смотреть
            // на угрозу, а не разворачивается вдоль вектора шага.
            float targetYaw = transform->rotation.y;
            if (NpcCombatMath::TryTargetYaw(
                target->position.x - transform->position.x,
                target->position.z - transform->position.z,
                targetYaw))
            {
                transform->rotation.y = targetYaw;
            }

            dodge.timeRemaining -= (std::max)(0.0f, deltaTime);
            if (dodge.timeRemaining <= 0.0f)
                it = s_dodges.erase(it);
            else
                ++it;
        }
    }

    static bool HasActiveDodge(Entity npc)
    {
        return s_dodges.find(npc) != s_dodges.end();
    }

    // Сбрасывает только активный эксперимент Combat Lab. Обычное решение AI
    // не затрагивается, поэтому смена профиля безопасна и воспроизводима.
    static bool ClearForcedRuntime(ECSRegistry& registry, Entity npc)
    {
        bool cleared = s_dodges.erase(npc) > 0;

        if (auto* modules =
            registry.GetComponent<NpcCombatModulesComponent>(npc))
        {
            if (modules->runtimeForcedTime > 0.0f)
                cleared = true;
            modules->ClearDebugOverride();
        }

        if (auto* tactical = registry.GetComponent<NpcTacticalComponent>(npc))
        {
            if (tactical->debugForcedCover)
            {
                NpcCoverLifecycle::Clear(*tactical);
                cleared = true;
            }
            else if (tactical->debugForcedCoverAction)
            {
                tactical->debugForcedCoverAction = false;
                tactical->movingToPeek = false;
                tactical->isPeeking = false;
                tactical->wantsPeek = false;
                tactical->exposed = false;
                tactical->peekState = NpcTacticalComponent::PeekSide::none;
                cleared = true;
            }
        }

        if (!cleared)
            return false;

        if (auto* movement = registry.GetComponent<CharacterMovement>(npc))
            StopMovement(*movement);
        if (auto* brain = registry.GetComponent<NpcBrainComponent>(npc))
            brain->state = NpcState::Idle;
        NpcNavMeshSystem::InvalidatePath(npc);
        return true;
    }

    static void ClearAll(ECSRegistry& registry)
    {
        for (const auto& [npc, dodge] : s_dodges)
        {
            (void)dodge;
            if (auto* movement = registry.GetComponent<CharacterMovement>(npc))
                StopMovement(*movement);
        }
        s_dodges.clear();
        registry.ForEach<NpcCombatModulesComponent>(
            [&](Entity npc, NpcCombatModulesComponent& modules)
            {
                const bool hadDebugOverride = modules.runtimeForcedTime > 0.0f;
                modules.ClearDebugOverride();
                if (!hadDebugOverride)
                    return;

                if (auto* movement =
                    registry.GetComponent<CharacterMovement>(npc))
                {
                    StopMovement(*movement);
                }
                if (auto* brain = registry.GetComponent<NpcBrainComponent>(npc))
                    brain->state = NpcState::Idle;
                NpcNavMeshSystem::InvalidatePath(npc);
            });
        registry.ForEach<NpcTacticalComponent>(
            [&](Entity npc, NpcTacticalComponent& tactical)
            {
                if (!tactical.debugForcedCoverAction)
                    return;

                if (tactical.debugForcedCover)
                {
                    NpcCoverLifecycle::Clear(tactical);
                }
                else
                {
                    tactical.debugForcedCoverAction = false;
                    tactical.movingToPeek = false;
                    tactical.isPeeking = false;
                    tactical.wantsPeek = false;
                    tactical.exposed = false;
                    tactical.peekState = NpcTacticalComponent::PeekSide::none;
                }
                if (auto* movement =
                    registry.GetComponent<CharacterMovement>(npc))
                {
                    StopMovement(*movement);
                }
                if (auto* brain = registry.GetComponent<NpcBrainComponent>(npc))
                    brain->state = NpcState::Idle;
                NpcNavMeshSystem::InvalidatePath(npc);
            });
    }

private:
    struct ActiveDodge
    {
        bool left = true;
        float timeRemaining = 0.55f;
    };

    inline static std::unordered_map<Entity, ActiveDodge> s_dodges;

    static bool Execute(
        ECSRegistry& registry,
        btDynamicsWorld* world,
        const NpcDebugCombatActionRequest& request,
        std::string& result)
    {
        if (request.npc == INVALID_ENTITY ||
            !registry.IsAlive(request.npc) ||
            !registry.HasComponent<NPCComponent>(request.npc))
        {
            result = "Select a live NPC first";
            return false;
        }

        auto* combat = registry.GetComponent<NPCCombatComponent>(request.npc);
        auto* brain = registry.GetComponent<NpcBrainComponent>(request.npc);
        auto* transform = registry.GetComponent<TransformComponent>(request.npc);
        auto* movement = registry.GetComponent<CharacterMovement>(request.npc);
        auto* tactical = registry.GetComponent<NpcTacticalComponent>(request.npc);
        auto* modules =
            registry.GetComponent<NpcCombatModulesComponent>(request.npc);
        if (!combat || !brain || !transform || !movement)
        {
            result = "NPC is missing combat, brain, transform or movement data";
            return false;
        }

        if (request.action == NpcDebugCombatAction::CancelForcedAction)
        {
            ClearForcedRuntime(registry, request.npc);
            StopMovement(*movement);
            if (tactical)
            {
                tactical->movingToPeek = false;
                tactical->isPeeking = false;
                tactical->wantsPeek = false;
                tactical->exposed = false;
                tactical->peekState = NpcTacticalComponent::PeekSide::none;

                // Удаляем только приказ, которым владеет Combat Lab. Укрытие,
                // выбранное обычным AI, кнопка Cancel больше не разрушает.
                if (tactical->debugForcedCover)
                    NpcCoverLifecycle::Clear(*tactical);
                tactical->debugForcedCoverAction = false;
            }
            brain->state = NpcState::Idle;
            NpcNavMeshSystem::InvalidatePath(request.npc);
            result = "Forced action cleared; NPC returned to Idle";
            return true;
        }

        // Каждая новая кнопка начинает отдельный эксперимент. Иначе быстрый
        // переход Chase -> Fire оставил бы одновременно открытыми Pursuit,
        // Engagement и StationaryFire и результат снова стал бы неоднозначным.
        if (modules)
            modules->ClearDebugOverride();

        if (request.action == NpcDebugCombatAction::AcquireNearestTarget)
        {
            const Entity target = FindNearestEnemy(registry, request.npc);
            if (target == INVALID_ENTITY)
            {
                result = "No live enemy with a faction was found";
                return false;
            }
            const auto* targetTransform =
                registry.GetComponent<TransformComponent>(target);
            combat->targetEntity = target;
            combat->targetVisible = false;
            combat->ObserveVisibleTarget(
                targetTransform->position,
                0.0f,
                false);
            result = "Nearest enemy stored as a debug target; visibility was not forged";
            return true;
        }

        const auto* target = GetTargetTransform(registry, *combat);
        if (!target)
        {
            result = "Acquire a live target before running this command";
            return false;
        }

        switch (request.action)
        {
        case NpcDebugCombatAction::FaceTarget:
        {
            float targetYaw = transform->rotation.y;
            if (!NpcCombatMath::TryTargetYaw(
                target->position.x - transform->position.x,
                target->position.z - transform->position.z,
                targetYaw))
            {
                result = "Target is too close to define a facing direction";
                return false;
            }
            transform->rotation.y = targetYaw;
            StopMovement(*movement);
            result = "NPC faced the selected target immediately";
            return true;
        }

        case NpcDebugCombatAction::ChaseTarget:
            combat->lastKnownPosition = target->position;
            combat->hasLastKnownPosition = true;
            if (modules)
            {
                modules->ForceForDebug(
                    NpcCombatModuleBit(NpcCombatModule::Pursuit),
                    6.0f);
            }
            brain->state = NpcState::Chasing;
            NpcNavMeshSystem::InvalidatePath(request.npc);
            result = "Chase state forced toward the selected target";
            return true;

        case NpcDebugCombatAction::HoldPosition:
            StopMovement(*movement);
            brain->state = NpcState::Idle;
            result = "Movement stopped; NPC returned to Idle without enabling fire";
            return true;

        case NpcDebugCombatAction::FireStance:
            if (!combat->targetVisible)
            {
                result = "Fire stance requires a target visible in the current frame";
                return false;
            }
            StopMovement(*movement);
            if (modules)
            {
                const std::uint32_t fireMask =
                    NpcCombatModuleBit(NpcCombatModule::Engagement) |
                    NpcCombatModuleBit(NpcCombatModule::StationaryFire);
                modules->ForceForDebug(
                    fireMask,
                    (std::max)(2.0f, combat->rangedAimDelay + 1.5f));
            }
            brain->state = NpcState::Attacking;
            combat->rangedAimTime = 0.0f;
            result = "Firing stance forced; normal aim, muzzle and weapon checks remain active";
            return true;

        case NpcDebugCombatAction::DodgeLeft:
        case NpcDebugCombatAction::DodgeRight:
            s_dodges[request.npc] = {
                request.action == NpcDebugCombatAction::DodgeLeft,
                0.55f
            };
            result = request.action == NpcDebugCombatAction::DodgeLeft
                ? "Forced a 0.55 second left dodge"
                : "Forced a 0.55 second right dodge";
            return true;

        case NpcDebugCombatAction::FindCover:
            return FindAndAssignCover(
                registry,
                world,
                request.npc,
                *combat,
                *brain,
                *transform,
                tactical,
                false,
                result);

        case NpcDebugCombatAction::PeekLeft:
            if (!tactical ||
                (tactical->reservedCover == INVALID_ENTITY &&
                    !tactical->dynamicCoverActive))
            {
                result = "Peek requires an assigned authored or dynamic cover";
                return false;
            }
            tactical->movingToPeek = true;
            tactical->debugForcedCoverAction = true;
            tactical->wantsPeek = true;
            tactical->isPeeking = false;
            tactical->exposed = false;
            tactical->peekState = NpcTacticalComponent::PeekSide::none;
            tactical->moveTarget = tactical->peekPosition;
            brain->state = NpcState::TakingCover;
            NpcNavMeshSystem::InvalidatePath(request.npc);
            result = "Left peek route forced through the normal cover executor";
            return true;

        case NpcDebugCombatAction::ReturnBehindCover:
            if (!tactical ||
                (tactical->reservedCover == INVALID_ENTITY &&
                    !tactical->dynamicCoverActive))
            {
                result = "Return requires an assigned cover";
                return false;
            }
            tactical->movingToPeek = false;
            tactical->debugForcedCoverAction = true;
            tactical->wantsPeek = false;
            tactical->isPeeking = false;
            tactical->exposed = false;
            tactical->peekState = NpcTacticalComponent::PeekSide::none;
            tactical->moveTarget = tactical->coverPosition;
            brain->state = NpcState::TakingCover;
            NpcNavMeshSystem::InvalidatePath(request.npc);
            result = "Return behind cover forced through the normal movement path";
            return true;

        case NpcDebugCombatAction::Reposition:
            return FindAndAssignCover(
                registry,
                world,
                request.npc,
                *combat,
                *brain,
                *transform,
                tactical,
                true,
                result);

        default:
            result = "Unsupported Combat Lab command";
            return false;
        }
    }

    static Entity FindNearestEnemy(ECSRegistry& registry, Entity npc)
    {
        const auto* observerFaction = registry.GetComponent<FactionComponent>(npc);
        const auto* observerTransform = registry.GetComponent<TransformComponent>(npc);
        if (!observerFaction || !observerTransform)
            return INVALID_ENTITY;

        Entity best = INVALID_ENTITY;
        float bestDistanceSquared = (std::numeric_limits<float>::max)();
        registry.ForEach<FactionComponent, TransformComponent>(
            [&](Entity candidate,
                FactionComponent& faction,
                TransformComponent& candidateTransform)
            {
                if (candidate == npc ||
                    !registry.HasComponent<ActiveComponent>(candidate) ||
                    (!registry.HasComponent<NPCComponent>(candidate) &&
                        !registry.HasComponent<PlayerComponent>(candidate)) ||
                    !AreEnemies(observerFaction->faction, faction.faction))
                {
                    return;
                }
                const auto* vitals =
                    registry.GetComponent<CharacterVitalsComponent>(candidate);
                if (!vitals || vitals->health <= 0.0f)
                    return;
                const float distanceSquared = NpcCombatMath::DistanceSquared(
                    observerTransform->position,
                    candidateTransform.position);
                if (distanceSquared < bestDistanceSquared)
                {
                    bestDistanceSquared = distanceSquared;
                    best = candidate;
                }
            });
        return best;
    }

    static const TransformComponent* GetTargetTransform(
        ECSRegistry& registry,
        const NPCCombatComponent& combat)
    {
        if (combat.targetEntity == INVALID_ENTITY ||
            !registry.IsAlive(combat.targetEntity))
        {
            return nullptr;
        }
        return registry.GetComponent<TransformComponent>(combat.targetEntity);
    }

    static bool FindAndAssignCover(
        ECSRegistry& registry,
        btDynamicsWorld* world,
        Entity npc,
        NPCCombatComponent& combat,
        NpcBrainComponent& brain,
        const TransformComponent& transform,
        NpcTacticalComponent* tactical,
        bool rejectCurrent,
        std::string& result)
    {
        if (!world || !tactical || !combat.hasLastKnownPosition)
        {
            result = "Cover search requires physics, tactical data and target memory";
            return false;
        }

        const Entity previousRejectedCover = tactical->rejectedCover;
        const float previousRejectedTimer = tactical->rejectedCoverTimer;
        const bool hadCurrentCover =
            tactical->reservedCover != INVALID_ENTITY ||
            tactical->dynamicCoverActive;
        const DirectX::XMFLOAT3 previousCoverPosition =
            tactical->coverPosition;
        if (rejectCurrent && tactical->reservedCover != INVALID_ENTITY)
        {
            tactical->rejectedCover = tactical->reservedCover;
            tactical->rejectedCoverTimer = 4.0f;
        }

        const NpcCoverSearchResult search = NpcCoverQuery::FindBest(
            registry,
            combat.targetEntity,
            brain.currentRoom,
            transform.position,
            combat.lastKnownPosition,
            *tactical,
            world,
            true,
            rejectCurrent && hadCurrentCover
                ? &previousCoverPosition
                : nullptr);
        if (!search.found)
        {
            tactical->rejectedCover = previousRejectedCover;
            tactical->rejectedCoverTimer = previousRejectedTimer;
            result = rejectCurrent
                ? "No replacement passed the checks; current cover was kept"
                : "No valid cover passed protection, sight and path checks";
            return false;
        }

        NpcCoverLifecycle::Assign(
            *tactical,
            search.coverEntity,
            search.dynamic,
            search.coverPosition,
            search.peekPosition,
            search.crouched,
            transform.position,
            true);
        brain.state = NpcState::TakingCover;
        NpcNavMeshSystem::InvalidatePath(npc);
        result = search.dynamic
            ? "Dynamic cover found and assigned"
            : "Authored cover found and assigned";
        return true;
    }

    static void StopMovement(CharacterMovement& movement)
    {
        movement.walkDirection = {};
        movement.isMoving = false;
        movement.wantsRun = false;
        movement.isRunning = false;
        movement.currentSpeed = 0.0f;
        movement.locomotionState = PlayerLocomotionState::Idle;
    }
};
