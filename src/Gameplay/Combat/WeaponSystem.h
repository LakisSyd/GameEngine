#pragma once

#include <algorithm>

#include "WeaponComponent.h"
#include "DamageEvent.h"
#include "DamageSystem.h"

#include <ECSRegistry.h>
#include <Components.h>
#include <InputClass.h>

#include <Physics/PhysicsRaycast.h>
#include <MeshCuttingTest.h>
#include <JobSystem.h>

class WeaponSystem
{
public:
    static Entity GetEquippedWeaponEntity(ECSRegistry& registry, Entity owner)
    {
        if (owner == INVALID_ENTITY)
            return INVALID_ENTITY;

        if (auto* equipped = registry.GetComponent<EquippedWeaponComponent>(owner))
        {
            if (equipped->weapon != INVALID_ENTITY &&
                registry.HasComponent<WeaponComponent>(equipped->weapon))
            {
                return equipped->weapon;
            }
        }

        // Legacy fallback for scenes that still have WeaponComponent on the
        // player. EnterPlayMode migrates this into a real weapon entity.
        if (registry.HasComponent<WeaponComponent>(owner))
            return owner;

        return INVALID_ENTITY;
    }

    static void Update(
        ECSRegistry& registry,
        DamageSystem& damageSystem,
        InputClass* input,
        CameraClass* camera,
        btDynamicsWorld* world,
        ID3D11Device* device,
        JobSystem& jobs,
        float deltaTime)
    {
        if (!input || !camera || !world || !device)
            return;

        Entity playerEntity = INVALID_ENTITY;

        registry.ForEach<PlayerGameTag>(
            [&](Entity entity, PlayerGameTag&)
            {
                if (playerEntity == INVALID_ENTITY)
                    playerEntity = entity;
            });

        if (playerEntity == INVALID_ENTITY)
            return;

        const Entity weaponEntity = GetEquippedWeaponEntity(registry, playerEntity);
        auto* weapon = registry.GetComponent<WeaponComponent>(weaponEntity);

        if (!weapon || weapon->fireRate <= 0.0f)
            return;

        // LMB currently controls only the weapon pose. Actual damage remains on
        // the legacy H test path until projectile/hitscan firing is enabled.
        weapon->triggerHeld = input->IsLeftButtonDown();

        weapon->currentCooldown = (std::max)(
            0.0f,
            weapon->currentCooldown - deltaTime);

        const bool wantsToFire = input->IsKeyDown('H');

        if (!wantsToFire || weapon->currentCooldown > 0.0f)
            return;

        // A shot consumes its cooldown even when it misses. Otherwise holding the
        // trigger would cast a ray every frame until something enters the crosshair.
        weapon->currentCooldown = 1.0f / weapon->fireRate;

        const btCollisionObject* playerObject = nullptr;
        if (auto* controller = registry.GetComponent<CharacterController>(playerEntity))
            playerObject = controller->ghostObj;

        PhysicsRaycastHit rayHit;
        if (!PhysicsRaycast::CastFromCamera(
                camera,
                world,
                weapon->range,
                playerEntity,
                playerObject,
                rayHit))
        {
            return;
        }

        DamageEvent damageEvent;
        damageEvent.source = weaponEntity;
        damageEvent.target = rayHit.entity;
        damageEvent.damageAmount = weapon->damage;
        damageEvent.damageType = DamageEvent::DamageType::Physical;
        damageEvent.hit_position = rayHit.position;
        damageEvent.hit_normal = rayHit.normal;
        damageEvent.breachRadius = weapon->breachRadius;

        if (registry.HasComponent<HullPanelComponent>(rayHit.entity))
        {
            auto* panel = registry.GetComponent<HullPanelComponent>(rayHit.entity);
            damageEvent.canDamageHull = weapon->CanDamageHull();

            // Weapon type grants permission to damage armor. Remaining structural
            // health and alloy resistance decide whether this shot penetrates.
            const float effectiveDamage = panel->CalculateHullDamage(weapon->damage);
            const bool reachesBreachThreshold = panel->health - effectiveDamage <= 0.0f;

            if (damageEvent.canDamageHull &&
                panel->CanCreateBreach() &&
                reachesBreachThreshold)
            {
                const CameraClass::Ray cutRay{
                    camera->GetPosition(),
                    camera->GetForward()
                };

                // This calls the same asynchronous cutter as F6. The only gameplay
                // difference is the explicit caliber-sized radius from the weapon.
                damageEvent.canBreachHull = MeshCuttingTest::CutByRayAsync(
                    registry,
                    device,
                    jobs,
                    cutRay,
                    weapon->breachRadius,
                    true,
                    &damageEvent.meshCutOperationId);
            }
        }

        damageSystem.AddDamageEvent(damageEvent);
    }
};
