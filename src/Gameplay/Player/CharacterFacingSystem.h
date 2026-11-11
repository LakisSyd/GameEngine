#pragma once

#include "Components.h"
#include "ECSRegistry.h"
#include "NPCComponents.h"
#include "Gameplay/AI/NpcCombatDecisionRules.h"
#include "Gameplay/AI/NpcCombatMath.h"
#include "Gameplay/AI/NpcCombatModules.h"
#include <DirectXMath.h>
#include <cmath>
constexpr float ModelForwardOffset = DirectX::XM_PI; // 180 degrees, model faces backward.

class CharacterFacingSystem
{
public:
    static void Update(ECSRegistry& registry, float deltaTime)
    {
        registry.ForEach<CharacterMovement>(
            [&](Entity entity, CharacterMovement& movement)
            {
                if (!registry.HasComponent<ActiveComponent>(entity))
                    return;
                auto* transform = registry.GetComponent<TransformComponent>(entity);
                if (!transform)
                    return;

                DirectX::XMFLOAT3 dir = movement.walkDirection;
                bool hasFacingDirection = movement.isMoving;
                // Поворот корпуса не обязан совпадать с вектором перемещения.
                // Во время боевого обхода сохраняем внимание к известной цели,
                // а не читаем текущие координаты скрытого за стеной игрока.
                const auto* combat = registry.GetComponent<NPCCombatComponent>(entity);
                const auto* brain = registry.GetComponent<NpcBrainComponent>(entity);
                const auto* tactical =
                    registry.GetComponent<NpcTacticalComponent>(entity);
                const auto* modules =
                    registry.GetComponent<NpcCombatModulesComponent>(entity);
                bool combatAttentionEnabled = false;
                if (brain)
                {
                    switch (brain->state)
                    {
                    case NpcState::Chasing:
                        combatAttentionEnabled = NpcCombatModuleOperational(
                            modules,
                            NpcCombatModule::Pursuit);
                        break;
                    case NpcState::Searching:
                        combatAttentionEnabled = NpcCombatModuleOperational(
                            modules,
                            NpcCombatModule::Search);
                        break;
                    case NpcState::Attacking:
                        combatAttentionEnabled = NpcCombatModuleOperational(
                            modules,
                            NpcCombatModule::Engagement);
                        break;
                    case NpcState::TakingCover:
                    case NpcState::InCover:
                        // Это исполнитель уже выданного пути, в том числе
                        // ручной команды Combat Lab, а не автопоиск укрытия.
                        combatAttentionEnabled = true;
                        break;
                    default:
                        break;
                    }
                }
                const bool followsCombatMemory = combat && brain &&
                    combat->hasLastKnownPosition && !brain->command.IsActive() &&
                    combatAttentionEnabled &&
                    (brain->state == NpcState::Chasing || brain->state == NpcState::TakingCover ||
                     brain->state == NpcState::Searching || brain->state == NpcState::Attacking);

                const bool shouldFaceCombatAttention = followsCombatMemory &&
                    NpcCombatDecisionRules::ShouldFaceCombatAttention(
                        brain->state,
                        movement.isMoving,
                        tactical && tactical->movingToPeek);

                if (shouldFaceCombatAttention)
                {
                    const DirectX::XMFLOAT3 attention{combat->lastKnownPosition.x-transform->position.x,
                        0.0f, combat->lastKnownPosition.z-transform->position.z};
                    if (attention.x*attention.x+attention.z*attention.z > 0.000001f)
                    {
                        dir = attention;
                        hasFacingDirection = true;
                    }
                }
                else if (!movement.isMoving && tactical &&
                    NpcCombatModuleOperational(
                        modules,
                        NpcCombatModule::IncomingFireReaction) &&
                    tactical->hasIncomingFireDirection &&
                    tactical->incomingFireTurnTimer > 0.0f)
                {
                    // Попадание от близкого стрелка поворачивает даже
                    // неподвижного NPC. Это лишь направление: NpcCombatSystem
                    // всё ещё обязан подтвердить цель лучом зрения.
                    dir = tactical->incomingFireDirection;
                    hasFacingDirection = true;
                }
                if (!hasFacingDirection)
                    return;

                const float lenSq = dir.x * dir.x + dir.z * dir.z;
                if (lenSq < 0.0001f)
                    return;

                const float targetYaw =
                    NpcCombatMath::TargetYaw(dir.x, dir.z);

                const float currentYaw = transform->rotation.y;

                float deltaYaw = NpcCombatMath::ShortestYawDelta(
                    currentYaw,
                    targetYaw);

                const float maxStep =
                    movement.rotationSpeed * deltaTime;

                deltaYaw = std::clamp(
                    deltaYaw,
                    -maxStep,
                    maxStep
                );

                transform->rotation.y += deltaYaw;
            });
    }
};
