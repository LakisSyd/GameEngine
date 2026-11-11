#pragma once
#include "WeaponComponent.h"
#include "WeaponFiringRules.h"
#include "DamageSystem.h"
#include <ECSRegistry.h>
#include <Components.h>
#include "Gameplay/Player/GameplayInputSystem.h"
#include <Physics/PhysicsRaycast.h>
#include <PhysicsDebugRenderer.h>
#include <MeshCuttingTest.h>
#include <JobSystem.h>
#include <algorithm>
#include <limits>
#include <random>
#include "Gameplay/Effects/ParticleSystem.h"

class WeaponSystem
{
public:
    // Стрелок и оружие — разные сущности. Эта функция получает оружие владельца.
    static Entity GetEquippedWeaponEntity(ECSRegistry& registry, Entity owner)
    {
        if (!registry.IsAlive(owner)) return INVALID_ENTITY;
        if (const auto* equipped = registry.GetComponent<EquippedWeaponComponent>(owner))
            if (registry.HasComponent<WeaponComponent>(equipped->weapon))
                return equipped->weapon;
        // Совместимость со старыми сценами, где оружие лежит на самом персонаже.
        return registry.HasComponent<WeaponComponent>(owner) ? owner : INVALID_ENTITY;
    }

    // Common entry point for player input and future NPC decisions. It starts
    // the mechanical reload only; animation/tactical behaviour stays outside.
    static bool RequestReload(ECSRegistry& registry, Entity owner)
    {
        auto* weapon = registry.GetComponent<WeaponComponent>(
            GetEquippedWeaponEntity(registry, owner));
        return weapon && WeaponFiringRules::StartReload(*weapon);
    }

    // Выдаёт владельцу патроны в резерв экипированного оружия. Магазин здесь
    // намеренно не заполняется мгновенно: пустому оружию всё равно потребуется
    // штатная перезарядка с reloadTimer. Возвращаем фактически добавленное число,
    // чтобы UI, ящик боеприпасов или pickup могли показать точный результат.
    static int GiveAmmo(ECSRegistry& registry, Entity owner, int rounds)
    {
        if (rounds <= 0)
            return 0;

        auto* weapon = registry.GetComponent<WeaponComponent>(
            GetEquippedWeaponEntity(registry, owner));
        if (!weapon)
            return 0;

        // Защищаем int от переполнения, если debug-кнопку нажимают много раз.
        weapon->reserveAmmo = (std::max)(0, weapon->reserveAmmo);
        const int freeIntegerRange =
            (std::numeric_limits<int>::max)() - weapon->reserveAmmo;
        const int granted = (std::min)(rounds, freeIntegerRange);
        weapon->reserveAmmo += granted;
        return granted;
    }

    // Вызывается ОДИН раз за кадр симуляции, а не на каждую попытку выстрела.
    // Благодаря этому кулдаун идёт у NPC, игрока и временно убранного оружия.
    static void UpdateTimers(ECSRegistry& registry, float dt)
    {
        registry.ForEach<WeaponComponent>([&](Entity, WeaponComponent& weapon) {
            weapon.triggerHeld = false;
            WeaponFiringRules::AdvanceTimers(weapon, dt);
        });
    }

    // true = выстрел состоялся, включая промах. false = оружие не было готово
    // или луч/стрелок некорректны. Здесь нет ввода, камеры и выбора цели.
    static bool TryFire(
        ECSRegistry& registry, Entity shooterEntity,
        const XMFLOAT3& origin, const XMFLOAT3& direction,
        btDynamicsWorld* world, DamageSystem& damageSystem,
        PhysicsDebugRenderer* debugRenderer, const PhysicsDebugSettings& debugSettings,
        ID3D11Device* device, JobSystem& jobs)
    {
        if (!world || !registry.IsAlive(shooterEntity) ||
            !registry.HasComponent<ActiveComponent>(shooterEntity) ||
            !WeaponFiringRules::ValidRay(origin, direction)) return false;
        const Entity weaponEntity = GetEquippedWeaponEntity(registry, shooterEntity);
        auto* weapon = registry.GetComponent<WeaponComponent>(weaponEntity);
        if (!weapon) return false;
        if (weapon->ammoInMagazine <= 0)
        {
            WeaponFiringRules::StartReload(*weapon);
            return false;
        }
        if (!WeaponFiringRules::CanFire(*weapon)) return false;

        const auto* controller = registry.GetComponent<CharacterController>(shooterEntity);
        const btCollisionObject* shooterObject = controller ? controller->ghostObj : nullptr;
        XMFLOAT3 normalized{};
        DirectX::XMStoreFloat3(&normalized,
            DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&direction)));
        const auto* movement = registry.GetComponent<CharacterMovement>(shooterEntity);
        const float motionScale = WeaponFiringRules::MotionMultiplier(*weapon, movement && movement->isMoving);
        // Одна выборка на состоявшийся выстрел. Дебаг, урон и пробои используют этот же луч.
        static std::mt19937 random{ std::random_device{}() };
        std::uniform_real_distribution<float> sample(0.0f, 1.0f);
        const float radial = sample(random);
        const float angular = sample(random);
        const XMFLOAT3 muzzleDirection = normalized;
        normalized = WeaponFiringRules::ConeDirection(normalized,
            weapon->spreadDegrees * motionScale + weapon->currentRecoilDegrees, radial, angular);
        WeaponFiringRules::CommitShot(*weapon, motionScale);
        // Empty weapons begin their mechanical reload immediately. NPC tactics
        // will later decide how the character behaves during this timer.
        if (weapon->ammoInMagazine <= 0)
            WeaponFiringRules::StartReload(*weapon);
        if (weapon->muzzleFlashEnabled)
            if (auto* particles = ParticleSystem::GetActiveSystem())
                particles->EmitMuzzleFlash(origin, muzzleDirection, weapon->muzzleFlashScale);

        // Луч останавливается на ПЕРВОМ объекте. Стена перед целью принимает
        // попадание сама; урон не переносится на выбранного AI персонажа за ней.
        PhysicsRaycastHit rayHit{};
        const bool hit = PhysicsRaycast::CastRay(origin, normalized, world,
            weapon->range, shooterEntity, shooterObject, rayHit);
        if (debugRenderer && debugSettings.showWeaponShotRays)
        {
            const XMFLOAT3 end = rayHit.collisionObject ? rayHit.position
                : XMFLOAT3{ rayHit.rayEnd.x(), rayHit.rayEnd.y(), rayHit.rayEnd.z() };
            debugRenderer->AddWeaponShotRay(origin, end,
                rayHit.collisionObject != nullptr, debugSettings.weaponRayLifetime);
        }
        if (!hit) return true;

        DamageEvent damageEvent{};
        damageEvent.source = weaponEntity;
        damageEvent.target = rayHit.entity;
        damageEvent.damageAmount = weapon->damage;
        damageEvent.damageType = DamageEvent::DamageType::Physical;
        damageEvent.hit_position = rayHit.position;
        damageEvent.hit_normal = rayHit.normal;
        damageEvent.breachRadius = weapon->breachRadius;
        if (const auto* panel = registry.GetComponent<HullPanelComponent>(rayHit.entity))
        {
            damageEvent.canDamageHull = weapon->CanDamageHull();
            const float effectiveDamage = panel->CalculateHullDamage(weapon->damage);
            const bool reachesBreachThreshold = panel->health - effectiveDamage <= 0.0f;
            // Без D3D-устройства нельзя разрезать меш, но обычный урон остаётся.
            if (device && damageEvent.canDamageHull && panel->CanCreateBreach() && reachesBreachThreshold)
            {
                const CameraClass::Ray cutRay{ origin, normalized };
                damageEvent.canBreachHull = MeshCuttingTest::CutByRayAsync(registry,
                    device, jobs, cutRay, weapon->breachRadius, true, &damageEvent.meshCutOperationId);
            }
        }
        damageSystem.AddDamageEvent(damageEvent);
        return true;
    }

    // Адаптер игрока: читает кнопку и выбирает уже подготовленный луч от дула.
    // Сам выстрел выполняется тем же TryFire, который вызывает NPC.
    static void Update(
        ECSRegistry& registry, DamageSystem& damageSystem, const GameInputFrame& input,
        CameraClass* camera, btDynamicsWorld* world, ID3D11Device* device, JobSystem& jobs,
        PhysicsDebugRenderer* debugRenderer, const PhysicsDebugSettings& debugSettings,
        const CameraClass::Ray* weaponShotRay = nullptr, bool suppressFire = false)
    {
        if (!world || (!weaponShotRay && !camera)) return;
        Entity player = INVALID_ENTITY;
        registry.ForEach<PlayerGameTag>([&](Entity entity, PlayerGameTag&) {
            if (player == INVALID_ENTITY) player = entity;
        });
        auto* weapon = registry.GetComponent<WeaponComponent>(GetEquippedWeaponEntity(registry, player));
        if (!weapon) return;
        if (input.Pressed(GameAction::Reload))
            RequestReload(registry, player);
        weapon->triggerHeld = !weapon->isReloading &&
            !suppressFire && input.Down(GameAction::Fire);
        if (!weapon->triggerHeld) return;
        const CameraClass::Ray ray = weaponShotRay ? *weaponShotRay
            : CameraClass::Ray{ camera->GetPosition(), camera->GetForward() };
        TryFire(registry, player, ray.origin, ray.direction, world, damageSystem,
            debugRenderer, debugSettings, device, jobs);
    }
};
