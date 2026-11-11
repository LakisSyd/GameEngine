#include "DamageSystem.h"
#include "PlayerComponents.h"
#include "NPCComponents.h"
#include "Components.h"
#include "Gameplay/Repair/RepairComponent.h"
#include "Gameplay/Repair/RuntimeMeshDamageComponent.h"
#include "Gameplay/Ship/DecompressionSourceComponent.h"
#include "Gameplay/Combat/WeaponComponent.h"
#include "Gameplay/AI/NpcCombatDecisionRules.h"
#include <Gameplay/Effects/ParticleSystem.h>
#include <Gameplay/Effects/ExplosionSystem.h>
#include <algorithm>
#include <cmath>

namespace
{


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

    void AddOrStrengthenHullDecal(
        ECSRegistry& registry,
        const DamageEvent& event,
        float radius,
        float intensity)
    {
        auto* transform = registry.GetComponent<TransformComponent>(event.target);
        if (!transform)
            return;

        if (!registry.HasComponent<DamageDecalComponent>(event.target))
            registry.AddComponent<DamageDecalComponent>(event.target, DamageDecalComponent{});

        auto* decals = registry.GetComponent<DamageDecalComponent>(event.target);
        if (!decals)
            return;

        const DirectX::XMMATRIX inverseWorld = DirectX::XMMatrixInverse(
            nullptr,
            transform->GetWorldMatrix());

        DamageDecalComponent::CrackEntry entry;
        DirectX::XMStoreFloat3(
            &entry.localPosition,
            DirectX::XMVector3TransformCoord(
                DirectX::XMLoadFloat3(&event.hit_position),
                inverseWorld));
        DirectX::XMStoreFloat3(
            &entry.localNormal,
            DirectX::XMVector3Normalize(DirectX::XMVector3TransformNormal(
                DirectX::XMLoadFloat3(&event.hit_normal),
                inverseWorld)));
        entry.radius = radius;
        entry.intensity = intensity;

        // Stable position-based rotation prevents every generated mask from lining up.
        const float seed = std::sin(
            entry.localPosition.x * 12.9898f +
            entry.localPosition.y * 78.233f +
            entry.localPosition.z * 37.719f) * 43758.5453f;
        entry.rotation = (seed - std::floor(seed)) * DirectX::XM_2PI;

        DamageDecalComponent::CrackEntry* nearest = nullptr;
        float nearestDistanceSq = radius * radius * 0.25f;
        for (auto& crack : decals->cracks)
        {
            const float dx = crack.localPosition.x - entry.localPosition.x;
            const float dy = crack.localPosition.y - entry.localPosition.y;
            const float dz = crack.localPosition.z - entry.localPosition.z;
            const float distanceSq = dx * dx + dy * dy + dz * dz;
            if (distanceSq < nearestDistanceSq)
            {
                nearestDistanceSq = distanceSq;
                nearest = &crack;
            }
        }

        if (nearest)
        {
            nearest->intensity = (std::max)(nearest->intensity, entry.intensity);
            nearest->radius = (std::max)(nearest->radius, entry.radius);
            nearest->localNormal = entry.localNormal;
            return;
        }

        if (decals->cracks.size() >= DamageDecalComponent::MAX_CRACKS)
            decals->cracks.erase(decals->cracks.begin());
        decals->cracks.push_back(entry);
    }
}

void DamageSystem::Update(
    ECSRegistry& registry,
    ParticleSystem& particleSystem,
    WorldEventLog& eventLog,
    double worldTime,
    float deltaTime)
{
    (void)deltaTime;

    for (const auto& damageEvent : damageQueue)
    {

        WorldEvent event;

        if (damageEvent.target == INVALID_ENTITY || damageEvent.damageAmount <= 0.0f)
            continue;
        if (!registry.HasComponent<ActiveComponent>(damageEvent.target))
            continue;

        // Бессмертие запрещает уменьшать здоровье, но не отменяет реакцию
        // на обычное попадание. silentKill — единственное исключение:
        // будущая stealth-атака нанесёт урон, но не включит боевую тревогу.
        if (!damageEvent.silentKill)
        {
            if (auto* tactical =
                    registry.GetComponent<NpcTacticalComponent>(damageEvent.target))
            {
                tactical->underFireTime = 3.0f;
                // Это настоящий DamageEvent, а не отладочный near miss.
                // Счётчик обновляется на каждое попадание, даже если общий
                // reaction cooldown ещё не разрешает новое резкое решение.
                tactical->damageReactionWasHit = true;
                ++tactical->recentDamageHits;
                tactical->recentDamageWindow = 4.0f;
                if (tactical->damageReactionCooldown <= 0.0f)
                    tactical->damageReactionPending = true;

                // DamageEvent.source обычно указывает на Entity оружия.
                // Разрешаем её до владельца, но не записываем Entity в combat:
                // NPC получает только направление, а цель ещё нужно увидеть.
                Entity perceivedSource = damageEvent.source;
                if (perceivedSource != INVALID_ENTITY)
                {
                    if (const auto* owner =
                            registry.GetComponent<WeaponOwnerComponent>(perceivedSource))
                    {
                        perceivedSource = owner->owner;
                    }
                }

                const auto* victimTransform =
                    registry.GetComponent<TransformComponent>(damageEvent.target);
                const auto* sourceTransform = perceivedSource != INVALID_ENTITY
                    ? registry.GetComponent<TransformComponent>(perceivedSource)
                    : nullptr;
                if (perceivedSource != damageEvent.target &&
                    victimTransform && sourceTransform)
                {
                    const float dx = sourceTransform->position.x -
                        victimTransform->position.x;
                    const float dz = sourceTransform->position.z -
                        victimTransform->position.z;
                    const float distanceSquared = dx * dx + dz * dz;
                    const float awarenessRange = (std::max)(
                        0.0f,
                        tactical->incomingFireAwarenessRange);

                    // Выстрел за сотни метров не даёт точной ориентации.
                    // Близкий попавший стрелок даёт короткую память направления.
                    if (NpcCombatDecisionRules::CanReactToIncomingFireDirection(
                            damageEvent.silentKill,
                            distanceSquared,
                            awarenessRange))
                    {
                        const float inverseLength =
                            1.0f / std::sqrt(distanceSquared);
                        tactical->incomingFireDirection = {
                            dx * inverseLength,
                            0.0f,
                            dz * inverseLength };
                        tactical->hasIncomingFireDirection = true;
                        tactical->incomingFireTurnTimer = 1.5f;
                    }
                }
            }
        }

        // После регистрации тактической реакции бессмертие останавливает
        // именно последствия урона: здоровье, пробоины и уничтожение Entity.
        if (registry.HasComponent<InvulnerableComponent>(damageEvent.target))
            continue;

        if (registry.HasComponent<PlayerComponent>(damageEvent.target))
        {
            if (auto* vitals = registry.GetComponent<CharacterVitalsComponent>(damageEvent.target))
            {
                vitals->health -= damageEvent.damageAmount;
                vitals->Normalize();
            }
            continue;
        }

        if (auto* npc = registry.GetComponent<NPCComponent>(damageEvent.target);
            npc && !registry.HasComponent<DamageableComponent>(damageEvent.target))
        {
            auto* vitals = registry.GetComponent<CharacterVitalsComponent>(damageEvent.target);
            if (!vitals)
                continue;
            vitals->health -= damageEvent.damageAmount;
            vitals->Normalize();
            if (vitals->health <= 0.0f)
                registry.RemoveComponent<ActiveComponent>(damageEvent.target);
            continue;
        }

        if (auto* panel = registry.GetComponent<HullPanelComponent>(damageEvent.target))
        {
            // Handheld weapons still create impact feedback, but they cannot wear
            // down ship armor regardless of ordinary character damage.
            if (!damageEvent.canDamageHull)
            {
                particleSystem.EmitMetalImpact(damageEvent);
                continue;
            }

            const float healthBefore = panel->health;
            const float damageBefore01 = 1.0f - panel->Health01();
            const float effectiveDamage = panel->CalculateHullDamage(damageEvent.damageAmount);

            panel->health -= effectiveDamage;
            panel->health = (std::max)(0.0f, (std::min)(panel->health, panel->maxHealth));
            panel->accumulatedDamage += effectiveDamage;

            const float damageAfter01 = 1.0f - panel->Health01();

            if (damageEvent.canBreachHull)
            {

                panel->breachCount = (std::min)(panel->breachCount + 1, panel->maxBreaches);
                panel->isDestroyed = panel->breachCount >= panel->maxBreaches;

                Entity breachEntity = registry.CreateEntity();

                HullBreachComponent breach;
                breach.panel = damageEvent.target;
                breach.room = panel->room;
                breach.worldPosition = damageEvent.hit_position;
                breach.radius = damageEvent.breachRadius;
                breach.leakRate = 0.15f;
                breach.sealed = false;

                if (auto* panelTransform = registry.GetComponent<TransformComponent>(damageEvent.target))
                {
                    const DirectX::XMMATRIX inversePanelWorld = DirectX::XMMatrixInverse(
                        nullptr,
                        panelTransform->GetWorldMatrix());
                    DirectX::XMStoreFloat3(
                        &breach.localPosition,
                        DirectX::XMVector3TransformCoord(
                            DirectX::XMLoadFloat3(&damageEvent.hit_position),
                            inversePanelWorld));
                    DirectX::XMStoreFloat3(
                        &breach.localNormal,
                        DirectX::XMVector3Normalize(DirectX::XMVector3TransformNormal(
                            DirectX::XMLoadFloat3(&damageEvent.hit_normal),
                            inversePanelWorld)));
                }

                registry.AddComponent<HullBreachComponent>(breachEntity, breach);
                registry.AddComponent<DecompressionSourceComponent>(
                    breachEntity,
                    DecompressionSourceComponent{});
                registry.AddComponent<RepairableComponent>(breachEntity, RepairableComponent{});
                registry.AddComponent<HullBreachRepairLinkComponent>(
                    breachEntity,
                    HullBreachRepairLinkComponent{ damageEvent.meshCutOperationId });
                registry.AddComponent<ActiveComponent>(breachEntity, {});


                
                event.type = WorldEventType::BreachOpened;
                event.timeSeconds = worldTime;
                event.subject = breachEntity;
                event.room = panel->room;
                event.severity = damageEvent.breachRadius;
                eventLog.Push(event);

                // A large panel gets a fresh structural budget after each hole. This
                // is a simple precursor to independent per-position damage zones.
                if (!panel->isDestroyed)
                    panel->health = panel->maxHealth;

                AddOrStrengthenHullDecal(
                    registry,
                    damageEvent,
                    (std::max)(damageEvent.breachRadius * 1.45f, 0.35f),
                    1.0f);
                particleSystem.EmitHullBreach(damageEvent);
                OutputDebugStringA("[Damage] Hull breach created.\n");
                continue;
            }

            particleSystem.EmitMetalImpact(damageEvent);

            // Effects fire only when entering a stage, not on every later hit.
            if (damageBefore01 < 0.20f && damageAfter01 >= 0.20f)
            {
                AddOrStrengthenHullDecal(
                    registry,
                    damageEvent,
                    (std::max)(damageEvent.breachRadius * 0.85f, 0.25f),
                    0.38f);
                particleSystem.EmitHullDent(damageEvent);
            }

            if (damageBefore01 < 0.50f && damageAfter01 >= 0.50f)
            {
                AddOrStrengthenHullDecal(
                    registry,
                    damageEvent,
                    (std::max)(damageEvent.breachRadius * 1.15f, 0.35f),
                    0.82f);
                particleSystem.EmitHullCrack(damageEvent);
            }

            if (healthBefore > 0.0f && panel->health <= 0.0f)
                OutputDebugStringA("[Damage] Hull critical; penetration retry required.\n");

            continue;
        }

        auto* damageable = registry.GetComponent<DamageableComponent>(damageEvent.target);
        if (!damageable)
            continue;

        const bool wasAlive = damageable->IsAlive();
        damageable->health -= damageEvent.damageAmount;
        damageable->Normalize();
        if (registry.HasComponent<NPCComponent>(damageEvent.target))
        {
            if (auto* vitals = registry.GetComponent<CharacterVitalsComponent>(damageEvent.target))
            {
                vitals->health = damageable->health;
                vitals->Normalize();
            }
        }

        if (wasAlive && !damageable->IsAlive())
        {
            ExplosionSystem::Emit(
                registry,
                particleSystem,
                damageEvent.hit_position,
                ExplosionSettings{});

            event.type = WorldEventType::EquipmentDestroyed;
            event.timeSeconds = worldTime;
            event.source = damageEvent.source;
            event.subject = damageEvent.target;
            event.room = FindRoomForEntity(registry, damageEvent.target);
            event.severity = damageEvent.damageAmount;
            eventLog.Push(event);

        }
        
            
        else
            particleSystem.EmitMetalImpact(damageEvent);

        // После нового попадания устройство снова можно восстановить.
        if (auto* repairable = registry.GetComponent<RepairableComponent>(damageEvent.target))
        {
            repairable->progress = 0.0f;
            repairable->repaired = false;
        }
    }

    damageQueue.clear();
}
